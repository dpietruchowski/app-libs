#include "claudecliclient.h"

#include <memory>
#include <optional>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStringList>

#include "claudecli.h"

namespace
{
const int kTimeoutMs = 180000;

QString systemPrompt(const Messages& messages, const ToolsMap& toolsMap)
{
    QString prompt = QStringLiteral(
        "You stand in for a chat completions API. Answer as the assistant would, with the next "
        "message only, and no preamble about what you are doing.\n\n");

    prompt += claudecli::systemMessages(messages);

    if (!toolsMap.isEmpty())
    {
        const QJsonDocument tools(toJsonArray(toolsMap));
        prompt += QStringLiteral(
                      "Tools you may call:\n%1\n"
                      "To call one, your ENTIRE reply is one JSON object and nothing else:\n"
                      "{\"tool\": \"<name>\", \"arguments\": {...}}\n"
                      "No words before or after it, no markdown, no tags, no list, no state "
                      "name, no plan: anything outside the object is shown to the user and the "
                      "call is lost. One call per reply. Write the app yourself and pass it in "
                      "the arguments. When you are not calling a tool, reply with plain text.\n")
                      .arg(QString::fromUtf8(tools.toJson(QJsonDocument::Compact)));
    }

    return prompt.trimmed();
}

QString stripFence(const QString& text)
{
    QString trimmed = text.trimmed();
    if (!trimmed.startsWith(QStringLiteral("```")))
        return trimmed;

    const int firstBreak = trimmed.indexOf(QLatin1Char('\n'));
    if (firstBreak < 0)
        return trimmed;

    trimmed = trimmed.mid(firstBreak + 1);
    if (trimmed.endsWith(QStringLiteral("```")))
        trimmed.chop(3);
    return trimmed.trimmed();
}

using claudecli::failure;

int objectEnd(const QString& text, int start)
{
    int depth = 0;
    bool inString = false;
    for (int i = start; i < text.size(); ++i)
    {
        const QChar c = text.at(i);
        if (inString)
        {
            if (c == QLatin1Char('\\'))
                ++i;
            else if (c == QLatin1Char('"'))
                inString = false;
        }
        else if (c == QLatin1Char('"'))
            inString = true;
        else if (c == QLatin1Char('{'))
            ++depth;
        else if (c == QLatin1Char('}') && --depth == 0)
            return i;
    }
    return -1;
}

QVariantMap argumentsOf(const QJsonObject& object)
{
    for (const char* key : { "arguments", "parameters", "input" })
    {
        const QJsonValue value = object.value(QLatin1String(key));
        if (value.isObject())
            return value.toObject().toVariantMap();
        if (value.isString())
            return QJsonDocument::fromJson(value.toString().toUtf8()).object().toVariantMap();
    }
    return QVariantMap();
}

bool isFabricatedAnswer(const QJsonObject& object, const QVariantMap& arguments)
{
    for (const char* key : { "return", "result", "output", "response" })
    {
        if (object.contains(QLatin1String(key)) || arguments.contains(QLatin1String(key)))
            return true;
    }
    return false;
}

std::optional<ToolCall> toolCallFrom(const QJsonObject& object)
{
    QString name = object.value(QStringLiteral("tool")).toString();
    QVariantMap arguments = argumentsOf(object);

    const QJsonValue function = object.value(QStringLiteral("function"));
    if (name.isEmpty() && function.isObject())
    {
        name = function.toObject().value(QStringLiteral("name")).toString();
        if (arguments.isEmpty())
            arguments = argumentsOf(function.toObject());
    }
    if (name.isEmpty() && function.isString())
        name = function.toString();
    if (name.isEmpty() && !argumentsOf(object).isEmpty())
        name = object.value(QStringLiteral("name")).toString();
    if (name.isEmpty() && arguments.value(QStringLiteral("tool")).canConvert<QString>())
        name = arguments.take(QStringLiteral("tool")).toString();

    if (name.isEmpty() || isFabricatedAnswer(object, arguments))
        return std::nullopt;
    return ToolCall { .id = QString(),
                      .name = name,
                      .arguments = arguments,
                      .type = QStringLiteral("function") };
}

QJsonObject unclosedObject(const QString& text, int start, int& end)
{
    int depth = 0;
    bool inString = false;
    for (int i = start; i < text.size(); ++i)
    {
        const QChar c = text.at(i);
        if (inString)
        {
            if (c == QLatin1Char('\\'))
                ++i;
            else if (c == QLatin1Char('"'))
                inString = false;
        }
        else if (c == QLatin1Char('"'))
            inString = true;
        else if (c == QLatin1Char('{'))
            ++depth;
        else if (c == QLatin1Char('}') && --depth == 1)
        {
            const QByteArray fragment = text.mid(start, i - start + 1).toUtf8() + '}';
            const QJsonObject object = QJsonDocument::fromJson(fragment).object();
            if (!object.isEmpty())
            {
                end = i;
                return object;
            }
        }
    }
    end = -1;
    return QJsonObject();
}

std::optional<ToolCall> namedBeforeObject(const QString& text, int start, const QJsonObject& object,
                                          const QStringList& toolNames)
{
    if (object.isEmpty() || object.contains(QStringLiteral("tool"))
        || object.contains(QStringLiteral("function")))
        return std::nullopt;

    const int from = qMax(0, start - 160);
    const QString before = text.mid(from, start - from);
    QString found;
    int at = -1;
    for (const QString& name : toolNames)
    {
        const int index = before.lastIndexOf(name);
        if (index <= at)
            continue;
        const bool wordStart = index == 0 || !before.at(index - 1).isLetterOrNumber();
        const int after = index + name.size();
        const bool wordEnd = after >= before.size() || !before.at(after).isLetterOrNumber();
        if (wordStart && wordEnd)
        {
            at = index;
            found = name;
        }
    }
    if (found.isEmpty())
        return std::nullopt;

    return ToolCall { .id = QString(),
                      .name = found,
                      .arguments = object.toVariantMap(),
                      .type = QStringLiteral("function") };
}

QVector<ToolCall> toolCallsIn(const QString& answer, const QStringList& toolNames)
{
    QVector<ToolCall> calls;
    const QString text = stripFence(answer);
    int start = text.indexOf(QLatin1Char('{'));
    while (start >= 0)
    {
        int end = objectEnd(text, start);
        QJsonObject object;
        if (end >= 0)
            object = QJsonDocument::fromJson(text.mid(start, end - start + 1).toUtf8()).object();
        else
            object = unclosedObject(text, start, end);
        if (end < 0)
            break;
        std::optional<ToolCall> call = toolCallFrom(object);
        if (!call)
            call = namedBeforeObject(text, start, object, toolNames);
        if (call)
        {
            call->id = QStringLiteral("cli-%1-%2").arg(call->name).arg(calls.size() + 1);
            calls.append(*call);
            start = text.indexOf(QLatin1Char('{'), end + 1);
        }
        else
        {
            start = text.indexOf(QLatin1Char('{'), start + 1);
        }
    }
    return calls;
}

Completion completionFromAnswer(const QString& answer, const QStringList& toolNames)
{
    Completion completion;
    Choice choice;
    choice.index = 0;

    const QVector<ToolCall> calls = toolCallsIn(answer, toolNames);
    if (!calls.isEmpty())
    {
        choice.finish_reason = QStringLiteral("tool_calls");
        choice.message = Message { .role = QStringLiteral("assistant"), .content = QString() };
        choice.message.tool_calls = calls;
    }
    else
    {
        choice.finish_reason = QStringLiteral("stop");
        choice.message
            = Message { .role = QStringLiteral("assistant"), .content = answer.trimmed() };
    }

    completion.choices.append(choice);
    return completion;
}

Completion completionFromOutput(const QByteArray& output, const QStringList& toolNames)
{
    const QJsonObject object = QJsonDocument::fromJson(output).object();
    const QString answer = object.value(QStringLiteral("result")).toString();
    if (answer.isEmpty())
        return failure(claudecli::kServerError);

    if (object.value(QStringLiteral("is_error")).toBool())
        return failure(claudecli::kServerError);

    return completionFromAnswer(answer, toolNames);
}

QStringList arguments(const ModelConfig& config, const Messages& messages,
                      const ToolsMap& toolsMap)
{
    QStringList args { QStringLiteral("-p"),
                       QStringLiteral("--output-format"),
                       QStringLiteral("json"),
                       QStringLiteral("--model"),
                       claudecli::modelOf(config),
                       QStringLiteral("--safe-mode"),
                       QStringLiteral("--no-session-persistence") };

    const QString effort = claudecli::effortOf(config);
    if (!effort.isEmpty())
        args << QStringLiteral("--effort") << effort;

    args << QStringLiteral("--system-prompt") << systemPrompt(messages, toolsMap)
         << QStringLiteral("--tools") << QString();
    return args;
}

void ask(QProcess& process, const ModelConfig& config, const Messages& messages,
         const ToolsMap& toolsMap)
{
    process.start(ClaudeCliClient::executable(), arguments(config, messages, toolsMap));
    process.write(claudecli::conversation(messages).toUtf8());
    process.closeWriteChannel();
}
}

ClaudeCliClient::ClaudeCliClient()
    : Client(QString(), QStringLiteral("claude-cli"))
{
}

QString ClaudeCliClient::executable() { return claudecli::executable(); }

bool ClaudeCliClient::isAvailable() { return !executable().isEmpty(); }

void ClaudeCliClient::createCompletionAsync(const ModelConfig& config, const Messages& messages,
                                            const ToolsMap& toolsMap,
                                            const CompletionCreatedCallback& callback) const
{
    if (!isAvailable())
    {
        Completion completion = failure(claudecli::kNetworkError);
        callback(completion);
        return;
    }

    auto* process = new QProcess;
    auto answered = std::make_shared<bool>(false);

    QObject::connect(process, &QProcess::errorOccurred, process,
                     [process, callback, answered](QProcess::ProcessError)
                     {
                         if (*answered)
                             return;
                         *answered = true;
                         Completion completion = failure(claudecli::kNetworkError);
                         callback(completion);
                         process->deleteLater();
                     });

    QObject::connect(process, &QProcess::finished, process,
                     [process, callback, answered, names = toolsMap.keys()](int,
                                                                            QProcess::ExitStatus)
                     {
                         if (*answered)
                             return;
                         *answered = true;
                         Completion completion
                             = completionFromOutput(process->readAllStandardOutput(), names);
                         callback(completion);
                         process->deleteLater();
                     });

    ask(*process, config, messages, toolsMap);
}

Completion ClaudeCliClient::createCompletion(const ModelConfig& config, const Messages& messages,
                                             const ToolsMap& toolsMap) const
{
    if (!isAvailable())
        return failure(claudecli::kNetworkError);

    QProcess process;
    ask(process, config, messages, toolsMap);

    if (!process.waitForFinished(kTimeoutMs))
        return failure(claudecli::kServerError);

    return completionFromOutput(process.readAllStandardOutput(), toolsMap.keys());
}
