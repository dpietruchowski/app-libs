#pragma once

#include <QJsonArray>
#include <functional>
#include <memory>

#include "client.h"
#include "structures.h"

struct AgentObserver
{
    std::function<void(const ToolCall& call, ToolEffect effect)> toolStarted;
    std::function<void(const ToolCall& call, const QVariant& result)> toolFinished;
    std::function<void(const QString& answer)> finished;
    std::function<void(const QString& errorToken)> failed;
};

class Agent
{
public:
    using ResponseReceivedCallback = std::function<void(const QString& response)>;
    using ErrorReceivedCallback = std::function<void(const QString& error)>;
    Agent(const QString& model, const QString& systemPrompt);

    void setModel(const QString& model);
    void setReasoningEffort(const QString& effort);
    void setErrorCallback(ErrorReceivedCallback callback);
    void setObserver(AgentObserver observer);
    void setPartialTextCallback(Client::PartialTextCallback callback);

    void addInitialMessage(const QString& message);
    void addMessage(const QString& role, const QString& content);

    void requestAsync(const Client& client, const QString& message, const ResponseReceivedCallback& callback);
    void cancel();
    void clear();

    const Messages& messages() const;
    void addTool(const QString docstring, const Tool& tool);
    void addTool(const QString docstring, const Tool& tool, ToolEffect effect);

    bool asynRequestInProgress() const;

private:
    class LifeToken
    {
    public:
        LifeToken() = default;
        LifeToken(const LifeToken&) { }
        LifeToken& operator=(const LifeToken&)
        {
            expire();
            return *this;
        }

        std::weak_ptr<void> watch() const { return m_token; }
        void expire() { m_token = std::make_shared<char>(); }

    private:
        std::shared_ptr<void> m_token = std::make_shared<char>();
    };

    void onCompletionReceived(const Client& client, const Completion& completion);

    void createCompletionAsync(const Client& client);

    void handleToolCallsAsync(const Client& client, QVector<ToolCall> toolCalls, int index);
    void deliverResponse(const QString& response, const QString& errorToken = QString());

private:
    ModelConfig m_config;
    QString m_systemPrompt;
    ToolsMap m_toolsMap;
    Messages m_messages;
    ResponseReceivedCallback m_responseReceivedCallback;
    ErrorReceivedCallback m_errorCallback;
    AgentObserver m_observer;
    Client::PartialTextCallback m_partialTextCallback;
    LifeToken m_life;
    int m_toolCallsCount = 0;
};
