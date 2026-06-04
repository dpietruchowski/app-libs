#include "agentcontroller.h"

AgentController::AgentController(const QString& model, const QString& systemPrompt, QObject* parent)
    : QObject(parent)
    , m_agent(model, systemPrompt)
{
}

void AgentController::addTool(const QString docstring, const Tool& tool) { m_agent.addTool(docstring, tool); }

void AgentController::request(const Client& client, const QString& message)
{
    if (isBusy())
    {
        return;
    }

    m_agent.requestAsync(client, message,
                         [this](const QString& response)
                         {
                             m_lastResponse = response;
                             QMetaObject::invokeMethod(
                                 this,
                                 [this]
                                 {
                                     emit busyChanged();
                                     emit messagesChanged();
                                     emit requestFinished();
                                 },
                                 Qt::QueuedConnection);
                         });

    emit busyChanged();
    emit messagesChanged();
}

void AgentController::clear(const QString& initMessage)
{
    m_agent.clear();
    m_agent.addInitialMessage(initMessage);
    m_lastResponse = "";
    emit messagesChanged();
}

QString AgentController::lastResponse() const { return m_lastResponse; }

QVariantList AgentController::messages() const
{
    QVariantList chat;
    for (const auto& message : m_agent.messages())
    {
        if (message.role != "user" && message.role != "assistant")
        {
            continue;
        }
        if (message.content.isEmpty())
        {
            continue;
        }

        QVariantMap chatMessage;
        chatMessage["sender"] = message.role;
        chatMessage["text"] = message.content;
        chat.append(chatMessage);
    }
    return chat;
}

bool AgentController::isBusy() const { return m_agent.asynRequestInProgress(); }
