#include "claudecliclient.h"

#include <memory>

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
                      "To call one, reply with a single JSON object and nothing else:\n"
                      "{\"tool\": \"<name>\", \"arguments\": {...}}\n"
                      "Write the app yourself and pass it in the arguments. "
                      "Otherwise reply with plain text.\n")
                      .arg(QString::fromUtf8(tools.toJson(QJsonDocument::Compact)));
    }

    return prompt.trimmed();
}

QString conversation(const Messages& messages)
{
    QString prompt = QStringLiteral("Conversation so far:\n");
    for (const Message& message : messages)
    {
        if (message.role == QStringLiteral("system"))
            continue;
        prompt += roleLabel(message) + QLatin1Char(' ') + message.content + QLatin1Char('\n');
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

Completion completionFromAnswer(const QString& answer)
{
    Completion completion;
    Choice choice;
    choice.index = 0;

    const QJsonObject object = QJsonDocument::fromJson(stripFence(answer).toUtf8()).object();
    const QString tool = object.value(QStringLiteral("tool")).toString();

    if (!tool.isEmpty())
    {
        ToolCall call;
        call.id = QStringLiteral("cli-%1").arg(tool);
        call.name = tool;
        call.type = QStringLiteral("function");
        call.arguments = object.value(QStringLiteral("arguments")).toObject().toVariantMap();

        choice.finish_reason = QStringLiteral("tool_calls");
        choice.message = Message { .role = QStringLiteral("assistant"), .content = QString() };
        choice.message.tool_calls.append(call);
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
