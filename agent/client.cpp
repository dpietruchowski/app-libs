#include "client.h"
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QString>

#include "logging.h"
#include "structures.h"

namespace
{
static const char* kJsonModelKey = "model";
static const char* kJsonMessagesKey = "messages";
static const char* kJsonToolsKey = "tools";
}

Client::Client()
{
    if (m_apiKey.isEmpty())
    {
        m_apiKey = qgetenv("OPENAI_API_KEY");
    }
    manager = new QNetworkAccessManager();
}

Client::Client(const QString& url, const QString& apiKey)
    : m_url(url)
    , m_apiKey(apiKey)
{
    manager = new QNetworkAccessManager();
}

Client::~Client() { delete manager; }

void Client::createCompletionAsync(const QString& model, const Messages& messages, const ToolsMap& toolsMap,
                                   const CompletionCreatedCallback& callback) const
{
    QNetworkReply* reply = createRequest(model, messages, toolsMap);
    if (!reply)
    {
        return;
    }
    QObject::connect(reply, &QNetworkReply::finished, reply,
                     [this, reply, callback]
                     {
                         Completion completion = parseReply(reply);
                         callback(completion);
                     });
}

Completion Client::createCompletion(const QString& model, const Messages& messages, const ToolsMap& toolsMap) const
{
    auto* reply = createRequest(model, messages, toolsMap);
    QEventLoop eventLoop;
    QObject::connect(reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
    eventLoop.exec();

    return parseReply(reply);
}

QNetworkReply* Client::createRequest(const QString& model, const Messages& messages, const ToolsMap& toolsMap) const
{
    if (m_apiKey.isEmpty())
    {
        qCWarning(ClientLogic) << "Cannot make request with empty api key. "
                                  "Make sure OPENAI_API_KEY variable is set";
        return nullptr;
    }

    QNetworkRequest request(QUrl { m_url });
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(m_apiKey).toUtf8());

    QJsonObject json;
    json[kJsonModelKey] = model;
    json[kJsonMessagesKey] = toJsonArray(messages);

    QJsonArray tools = toJsonArray(toolsMap);
    if (!tools.empty())
    {
        json[kJsonToolsKey] = tools;
    }

    QJsonDocument doc(json);
    QByteArray data = doc.toJson();

    return manager->post(request, data);
}

Completion Client::parseReply(QNetworkReply* reply) const
{
    Completion completion;
    if (reply->error() == QNetworkReply::NoError)
    {
        QByteArray response = reply->readAll();
        qCDebug(ClientLogic) << "Response received successfully";

        QJsonDocument jsonResponse = QJsonDocument::fromJson(response);
        if (jsonResponse.isObject())
        {
            QJsonObject jsonObject = jsonResponse.object();
            completion = Completion::fromJsonObject(jsonObject);
        }
    }
    else
    {
        QString errorString = reply->errorString();
        QByteArray serverResponse = reply->readAll();
        qCWarning(ClientLogic) << "Error occurred:" << errorString << "Server replied:" << QString(serverResponse);
        completion.error = errorString;
    }
    delete reply;
    return completion;
}
