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
static const char* kJsonReasoningEffortKey = "reasoning_effort";
static const char* kJsonReasoningKey = "reasoning";
static const char* kJsonEffortKey = "effort";

QString classifyError(int httpStatus, const QByteArray& body)
{
    int code = httpStatus;
    if (code == 0)
    {
        const QJsonObject errorObject =
            QJsonDocument::fromJson(body).object().value(QStringLiteral("error")).toObject();
        code = errorObject.value(QStringLiteral("code")).toInt();
    }

    switch (code)
    {
    case 401:
    case 403:
        return QStringLiteral("invalid_key");
    case 402:
        return QStringLiteral("insufficient_credits");
    case 429:
        return QStringLiteral("rate_limit");
    default:
        break;
    }

    if (code >= 500 && code <= 599)
        return QStringLiteral("server_error");
    if (httpStatus == 0)
        return QStringLiteral("network_error");
    return QStringLiteral("unknown_error");
}
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

void Client::setReasoningEffort(const QString& effort, bool nested)
{
    m_reasoningEffort = effort;
    m_reasoningNested = nested;
}

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

    if (!m_reasoningEffort.isEmpty())
    {
        if (m_reasoningNested)
        {
            QJsonObject reasoning;
            reasoning[kJsonEffortKey] = m_reasoningEffort;
            json[kJsonReasoningKey] = reasoning;
        }
        else
        {
            json[kJsonReasoningEffortKey] = m_reasoningEffort;
        }
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
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QString errorString = reply->errorString();
        QByteArray serverResponse = reply->readAll();
        qCWarning(ClientLogic) << "Error occurred:" << errorString << "HTTP status:" << httpStatus
                               << "Server replied:" << QString(serverResponse);
        completion.error = classifyError(httpStatus, serverResponse);
    }
    delete reply;
    return completion;
}
