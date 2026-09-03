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

    virtual void createCompletionAsync(const ModelConfig& config, const Messages& messages,
                                       const ToolsMap& toolsMap,
                                       const CompletionCreatedCallback& callback) const;
    virtual Completion createCompletion(const ModelConfig& config, const Messages& messages,
                                        const ToolsMap& toolsMap) const;

    void setReasoningNested(bool nested);

private:
    QNetworkReply* createRequest(const ModelConfig& config, const Messages& messages,
                                 const ToolsMap& toolsMap) const;
    Completion parseReply(QNetworkReply* reply) const;

private:
    QNetworkAccessManager* manager;
    QString m_url = "https://api.openai.com/v1/chat/completions";
    QString m_apiKey;
    bool m_reasoningNested = false;
};
