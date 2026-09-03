#include "claudecliclient.h"

#include <memory>
#include <optional>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>

namespace
{
const int kTimeoutMs = 180000;

QString roleLabel(const Message& message)
{
    if (message.role == QStringLiteral("tool"))
        return QStringLiteral("[tool result]");
    return QStringLiteral("[%1]").arg(message.role);
}

QString systemPrompt(const Messages& messages, const ToolsMap& toolsMap)
{
    QString prompt = QStringLiteral(
        "You stand in for a chat completions API. Answer as the assistant would, with the next "
        "message only, and no preamble about what you are doing.\n\n");

    for (const Message& message : messages)
    {
        if (message.role == QStringLiteral("system"))
            prompt += message.content + QStringLiteral("\n\n");
    }

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

QString messageText(const Message& message)
{
    if (message.tool_calls.isEmpty())
        return message.content;

    QStringList calls;
    for (const ToolCall& call : message.tool_calls)
    {
        const QJsonObject object { { QStringLiteral("tool"), call.name },
                                   { QStringLiteral("arguments"),
                                     QJsonObject::fromVariantMap(call.arguments) } };
        calls.append(QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact)));
    }
    return calls.join(QLatin1Char('\n'));
}

QString conversation(const Messages& messages)
{
    QString prompt = QStringLiteral("Conversation so far:\n");
    for (const Message& message : messages)
    {
        if (message.role == QStringLiteral("system"))
            continue;
        prompt += roleLabel(message) + QLatin1Char(' ') + messageText(message) + QLatin1Char('\n');
    }
    return prompt;
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

Completion failure(const QString& error)
{
    Completion completion;
    completion.error = error;
    return completion;
}

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

    if (name.isEmpty())
        return std::nullopt;
    return ToolCall { .id = QString(),
                      .name = name,
                      .arguments = arguments,
                      .type = QStringLiteral("function") };
}

QVector<ToolCall> toolCallsIn(const QString& answer)
{
    QVector<ToolCall> calls;
    const QString text = stripFence(answer);
    int start = text.indexOf(QLatin1Char('{'));
    while (start >= 0)
    {
        const int end = objectEnd(text, start);
        if (end < 0)
            break;
        const QJsonObject object
            = QJsonDocument::fromJson(text.mid(start, end - start + 1).toUtf8()).object();
        if (std::optional<ToolCall> call = toolCallFrom(object))
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

Completion completionFromAnswer(const QString& answer)
{
    Completion completion;
    Choice choice;
    choice.index = 0;

    const QVector<ToolCall> calls = toolCallsIn(answer);
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

Completion completionFromOutput(const QByteArray& output)
{
    const QJsonObject object = QJsonDocument::fromJson(output).object();
    const QString answer = object.value(QStringLiteral("result")).toString();
    if (answer.isEmpty())
        return failure(QStringLiteral("server_error"));

    if (object.value(QStringLiteral("is_error")).toBool())
        return failure(QStringLiteral("server_error"));

    return completionFromAnswer(answer);
}

QStringList arguments(const Messages& messages, const ToolsMap& toolsMap)
{
    return { QStringLiteral("-p"),
             QStringLiteral("--output-format"),
             QStringLiteral("json"),
             QStringLiteral("--model"),
             QStringLiteral("haiku"),
             QStringLiteral("--safe-mode"),
             QStringLiteral("--no-session-persistence"),
             QStringLiteral("--system-prompt"),
             systemPrompt(messages, toolsMap),
             QStringLiteral("--tools"),
             QString() };
}

void ask(QProcess& process, const Messages& messages, const ToolsMap& toolsMap)
{
    process.start(ClaudeCliClient::executable(), arguments(messages, toolsMap));
    process.write(conversation(messages).toUtf8());
    process.closeWriteChannel();
}
}

ClaudeCliClient::ClaudeCliClient()
    : Client(QString(), QStringLiteral("claude-cli"))
{
}

QString ClaudeCliClient::executable()
{
    return QStandardPaths::findExecutable(QStringLiteral("claude"));
}

bool ClaudeCliClient::isAvailable() { return !executable().isEmpty(); }

void ClaudeCliClient::createCompletionAsync(const ModelConfig&, const Messages& messages,
                                            const ToolsMap& toolsMap,
                                            const CompletionCreatedCallback& callback) const
{
    if (!isAvailable())
    {
        Completion completion = failure(QStringLiteral("network_error"));
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
                         Completion completion = failure(QStringLiteral("network_error"));
                         callback(completion);
                         process->deleteLater();
                     });

    QObject::connect(process, &QProcess::finished, process,
                     [process, callback, answered](int, QProcess::ExitStatus)
                     {
                         if (*answered)
                             return;
                         *answered = true;
                         Completion completion
                             = completionFromOutput(process->readAllStandardOutput());
                         callback(completion);
                         process->deleteLater();
                     });

    ask(*process, messages, toolsMap);
}

Completion ClaudeCliClient::createCompletion(const ModelConfig&, const Messages& messages,
                                             const ToolsMap& toolsMap) const
{
    if (!isAvailable())
        return failure(QStringLiteral("network_error"));

    QProcess process;
    ask(process, messages, toolsMap);

    if (!process.waitForFinished(kTimeoutMs))
        return failure(QStringLiteral("server_error"));

    return completionFromOutput(process.readAllStandardOutput());
}
