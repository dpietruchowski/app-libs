#pragma once

#include <QString>
#include <functional>

#include "structures.h"

class QNetworkReply;
class QNetworkAccessManager;

QString classifyError(int httpStatus, const QByteArray& body);

class Client
{
public:
    using CompletionCreatedCallback = std::function<void(Completion& completion)>;
    using PartialTextCallback = std::function<void(const QString& textSoFar)>;

    Client();
    Client(const QString& url, const QString& apiKey);
    virtual ~Client();

    virtual void createCompletionAsync(const ModelConfig& config, const Messages& messages,
                                       const ToolsMap& toolsMap,
                                       const CompletionCreatedCallback& callback) const;
    virtual void createCompletionAsync(const ModelConfig& config, const Messages& messages,
                                       const ToolsMap& toolsMap,
                                       const CompletionCreatedCallback& callback,
                                       const PartialTextCallback& partialText) const;
    virtual Completion createCompletion(const ModelConfig& config, const Messages& messages,
                                        const ToolsMap& toolsMap) const;

    virtual QString session() const { return QString(); }

    void setReasoningNested(bool nested);

private:
    QNetworkReply* createRequest(const ModelConfig& config, const Messages& messages,
                                 const ToolsMap& toolsMap) const;
    Completion parseReply(QNetworkReply* reply) const;

protected:
    QNetworkAccessManager* manager;
    QString m_url = "https://api.openai.com/v1/chat/completions";
    QString m_apiKey;
    bool m_reasoningNested = false;
};
