#pragma once

#include <QString>
#include <functional>

#include "structures.h"

class QNetworkReply;
class QNetworkAccessManager;

class Client
{
public:
    using CompletionCreatedCallback = std::function<void(Completion& completion)>;

    Client();
    Client(const QString& url, const QString& apiKey);
    virtual ~Client();

    virtual void createCompletionAsync(const QString& model, const Messages& messages, const ToolsMap& toolsMap,
                                       const CompletionCreatedCallback& callback) const;
    virtual Completion createCompletion(const QString& model, const Messages& messages,
                                        const ToolsMap& toolsMap) const;

    void setReasoningEffort(const QString& effort, bool nested);

private:
    QNetworkReply* createRequest(const QString& model, const Messages& messages, const ToolsMap& toolsMap) const;
    Completion parseReply(QNetworkReply* reply) const;

private:
    QNetworkAccessManager* manager;
    QString m_url = "https://api.openai.com/v1/chat/completions";
    QString m_apiKey;
    QString m_reasoningEffort;
    bool m_reasoningNested = false;
};
