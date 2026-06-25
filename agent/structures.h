#pragma once

#include <functional>

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVariantMap>
#include <QVector>

using ToolDone = std::function<void(QVariant)>;
using Tool = std::function<void(QVariantMap, ToolDone)>;

struct ModelConfig
{
    QString model;
    QString reasoningEffort;
};

struct ToolData
{
    Tool tool;
    QJsonObject json;
};

using ToolsMap = QMap<QString, ToolData>;

QJsonArray toJsonArray(const ToolsMap& toolsMap);

struct ToolCall
{
    QString id;
    QString name;
    QVariantMap arguments;
    QString type;

    QJsonObject toJsonObject() const;
    static ToolCall fromJsonObject(const QJsonObject& json);
};

struct Message
{
    QString role;
    QString content;
    QString tool_call_id;
    QVector<ToolCall> tool_calls;

    QJsonObject toJsonObject() const;
    static Message fromJsonObject(const QJsonObject& json);
};

using Messages = QVector<Message>;

QJsonArray toJsonArray(const Messages& messages);
Messages fromJsonArray(const QJsonArray& json);

struct Usage
{
    int prompt_tokens;
    int completion_tokens;
    int total_tokens;

    QJsonObject toJsonObject() const;
    static Usage fromJsonObject(const QJsonObject& json);
};

struct Choice
{
    Message message;
    QString finish_reason;
    int index;

    QJsonObject toJsonObject() const;
    static Choice fromJsonObject(const QJsonObject& json);
};

struct Completion
{
    QString id;
    QString object;
    qint64 created;
    QString model;
    Usage usage;
    QVector<Choice> choices;
    QString error; // non-empty when the request failed; carries a category token
                   // (invalid_key, insufficient_credits, rate_limit, server_error,
                   // network_error, unknown_error), not raw server text

    QJsonObject toJsonObject() const;
    static Completion fromJsonObject(const QJsonObject& json);
};
