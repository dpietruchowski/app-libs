#pragma once

#include <QJsonArray>
#include <functional>

#include "client.h"
#include "structures.h"

class Agent
{
public:
    using ResponseReceivedCallback = std::function<void(const QString& response)>;
    Agent(const QString& model, const QString& systemPrompt);

    void addInitialMessage(const QString& message);

    void requestAsync(const Client& client, const QString& message, const ResponseReceivedCallback& callback);
    QString request(const Client& client, const QString& message);
    void clear();

    const Messages& messages() const;
    void addTool(const QString docstring, const Tool& tool);

    bool asynRequestInProgress() const;

private:
    QString onCompletionReceived(const Client& client, const Completion& completion, bool async = false);

    void createCompletionAsync(const Client& client);
    QString createCompletion(const Client& client);

    void handleToolCalls(const QVector<ToolCall>& toolCalls);

private:
    QString m_model;
    QString m_systemPrompt;
    ToolsMap m_toolsMap;
    Messages m_messages;
    ResponseReceivedCallback m_responseReceivedCallback;
    int m_toolCallsCount = 0;
};
