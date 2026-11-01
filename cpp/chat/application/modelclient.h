#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QString>
#include <memory>
#include <vector>

#include "chat/domain/message.h"
#include "chat/domain/toolspec.h"

namespace chat
{

struct ToolCall final
{
    QString id;
    QString name;
    QJsonObject arguments;
};

struct ToolOutcome final
{
    QJsonValue result;
    bool isError { false };
};

struct ModelRequest final
{
    QString instructions;
    QString model;
    QString reasoningEffort;
    std::vector<ToolSpec> tools;
    std::vector<Message> transcript;
    QString session;
    bool keepSession { false };
};

class ModelRun : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    virtual void answer(const QString& callId, const ToolOutcome& outcome) = 0;
    virtual void cancel() = 0;

signals:
    void sessionReady(const QString& session);
    void toolRequested(const chat::ToolCall& call);
    void finished(const QString& text);
    void failed(const QString& errorToken);
};

class ModelClient
{
public:
    virtual ~ModelClient() = default;

    virtual bool available() const = 0;
    virtual std::unique_ptr<ModelRun> start(const ModelRequest& request) = 0;
    virtual void forget(const QString& session) = 0;
};

}
