#pragma once

#include <QJsonArray>
#include <functional>

#include "client.h"
#include "structures.h"

class Agent
{
public:
    using ResponseReceivedCallback = std::function<void(const QString& response)>;
    using ErrorReceivedCallback = std::function<void(const QString& error)>;
    Agent(const QString& model, const QString& systemPrompt);

    void setReasoningEffort(const QString& effort);
    void setErrorCallback(ErrorReceivedCallback callback);

    void addInitialMessage(const QString& message);

    void requestAsync(const Client& client, const QString& message, const ResponseReceivedCallback& callback);
    void clear();

    const Messages& messages() const;
    void addTool(const QString docstring, const Tool& tool);

    bool asynRequestInProgress() const;

private:
    void onCompletionReceived(const Client& client, const Completion& completion);

    void createCompletionAsync(const Client& client);

    void handleToolCallsAsync(const Client& client, QVector<ToolCall> toolCalls, int index);
    void deliverResponse(const QString& response);

private:
    ModelConfig m_config;
    QString m_systemPrompt;
    ToolsMap m_toolsMap;
    Messages m_messages;
    ResponseReceivedCallback m_responseReceivedCallback;
    ErrorReceivedCallback m_errorCallback;
    int m_toolCallsCount = 0;
};
