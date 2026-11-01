#include "streamjsonparser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QStringList>
#include <utility>

#include "claudecli.h"

namespace
{

const QString kMcpMessage = QStringLiteral("mcp_message");
const QString kExecutionErrorSubtype = QStringLiteral("error_during_execution");
const QString kMessageStart = QStringLiteral("message_start");
const QString kContentBlockDelta = QStringLiteral("content_block_delta");
const QString kTextDelta = QStringLiteral("text_delta");

StreamEvent makeEvent(StreamEvent::Kind kind, QString text = {}, QString detail = {})
{
    StreamEvent event;
    event.kind = kind;
    event.text = std::move(text);
    event.detail = std::move(detail);
    return event;
}

QString failureDetail(const QJsonObject& resultEvent)
{
    const QString text = resultEvent.value("result").toString();
    if (!text.isEmpty())
        return text;

    QStringList errors;
    for (const QJsonValue& error : resultEvent.value("errors").toArray())
        errors.append(error.toString());
    return errors.join(QLatin1Char('\n'));
}

}

std::vector<StreamEvent> StreamJsonParser::feed(const QByteArray& chunk)
{
    std::vector<StreamEvent> events;
    if (m_finished)
        return events;

    m_buffer += chunk;
    qsizetype lineStart = 0;
    while (!m_finished)
    {
        const qsizetype lineEnd = m_buffer.indexOf('\n', lineStart);
        if (lineEnd < 0)
            break;
        handleLine(m_buffer.sliced(lineStart, lineEnd - lineStart), events);
        lineStart = lineEnd + 1;
    }

    if (m_finished)
        m_buffer.clear();
    else
        m_buffer.remove(0, lineStart);
    return events;
}

std::vector<StreamEvent> StreamJsonParser::endOfStream()
{
    std::vector<StreamEvent> events;
    if (m_finished)
        return events;

    handleLine(m_buffer, events);
    m_buffer.clear();
    if (!m_finished)
    {
        conclude(makeEvent(StreamEvent::Kind::Failure, claudecli::kServerError,
                           QStringLiteral("stream ended without a result")),
                 events);
    }
    return events;
}

bool StreamJsonParser::isFinished() const { return m_finished; }

void StreamJsonParser::handleLine(const QByteArray& line, std::vector<StreamEvent>& events)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return;

    const QJsonObject event = document.object();
    const QString type = event.value("type").toString();
    if (type == QStringLiteral("system"))
    {
        if (event.value("subtype").toString() == QStringLiteral("init"))
            handleInit(event, events);
    }
    else if (type == QStringLiteral("stream_event"))
        handleStreamEvent(event, events);
    else if (type == QStringLiteral("control_request"))
        handleControlRequest(event, events);
    else if (type == QStringLiteral("result"))
        handleResult(event, events);
}

void StreamJsonParser::handleInit(const QJsonObject& event, std::vector<StreamEvent>& events)
{
    m_initSeen = true;

    const QString sessionId = event.value("session_id").toString();
    if (!m_sessionReported && !sessionId.isEmpty())
    {
        m_sessionReported = true;
        events.push_back(makeEvent(StreamEvent::Kind::SessionStarted, sessionId));
    }
}

void StreamJsonParser::handleStreamEvent(const QJsonObject& event, std::vector<StreamEvent>& events)
{
    const QJsonObject inner = event.value("event").toObject();
    const QString innerType = inner.value("type").toString();
    if (innerType == kMessageStart)
    {
        m_messageText.clear();
        return;
    }

    const QJsonObject delta = inner.value("delta").toObject();
    if (innerType != kContentBlockDelta || delta.value("type").toString() != kTextDelta)
        return;

    m_messageText += delta.value("text").toString();
    events.push_back(makeEvent(StreamEvent::Kind::PartialText, m_messageText));
}

void StreamJsonParser::handleControlRequest(const QJsonObject& event,
                                            std::vector<StreamEvent>& events)
{
    const QJsonObject request = event.value("request").toObject();
    const QString requestId = event.value("request_id").toString();
    if (request.value("subtype").toString() != kMcpMessage || requestId.isEmpty())
        return;

    StreamEvent mcpRequest = makeEvent(StreamEvent::Kind::McpRequest);
    mcpRequest.requestId = requestId;
    mcpRequest.serverName = request.value("server_name").toString();
    mcpRequest.message = request.value("message");
    events.push_back(std::move(mcpRequest));
}

void StreamJsonParser::handleResult(const QJsonObject& event, std::vector<StreamEvent>& events)
{
    const QString subtype = event.value("subtype").toString();
    const bool succeeded
        = subtype == QStringLiteral("success") && !event.value("is_error").toBool();

    if (succeeded)
        conclude(makeEvent(StreamEvent::Kind::Answer, event.value("result").toString()), events);
    else if (!m_initSeen && subtype == kExecutionErrorSubtype)
        conclude(makeEvent(StreamEvent::Kind::SessionMissing, {}, failureDetail(event)), events);
    else
    {
        conclude(
            makeEvent(StreamEvent::Kind::Failure, claudecli::kServerError, failureDetail(event)),
            events);
    }
}

void StreamJsonParser::conclude(StreamEvent finalEvent, std::vector<StreamEvent>& events)
{
    events.push_back(std::move(finalEvent));
    m_finished = true;
}
