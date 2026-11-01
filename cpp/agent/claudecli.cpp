#include "claudecli.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStringList>

namespace
{
QString roleLabel(const Message& message)
{
    if (message.role == QStringLiteral("tool"))
        return QStringLiteral("[tool result]");
    return QStringLiteral("[%1]").arg(message.role);
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
}

namespace claudecli
{

Completion failure(const QString& error)
{
    Completion completion;
    completion.error = error;
    return completion;
}

bool isSessionId(const QString& id)
{
    static const QRegularExpression pattern(
        QRegularExpression::anchoredPattern(
            QStringLiteral("[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}")),
        QRegularExpression::CaseInsensitiveOption);
    return pattern.match(id).hasMatch();
}

QString executable() { return QStandardPaths::findExecutable(QStringLiteral("claude")); }

QString modelOf(const ModelConfig& config)
{
    static const QStringList aliases { QStringLiteral("haiku"),   QStringLiteral("sonnet"),
                                       QStringLiteral("opus"),    QStringLiteral("fable"),
                                       QStringLiteral("default"), QStringLiteral("sonnet[1m]"),
                                       QStringLiteral("opus[1m]") };
    const QString model = config.model.toLower();
    if (aliases.contains(model) || model.startsWith(QStringLiteral("claude")))
        return model;
    return QStringLiteral("haiku");
}

QString effortOf(const ModelConfig& config)
{
    static const QStringList levels { QStringLiteral("low"), QStringLiteral("medium"),
                                      QStringLiteral("high"), QStringLiteral("xhigh"),
                                      QStringLiteral("max") };
    const QString effort = config.reasoningEffort.toLower();
    return levels.contains(effort) ? effort : QString();
}

QString systemMessages(const Messages& messages)
{
    QString text;
    for (const Message& message : messages)
    {
        if (message.role == QStringLiteral("system"))
            text += message.content + QStringLiteral("\n\n");
    }
    return text;
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

}
