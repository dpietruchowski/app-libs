#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <vector>

struct StreamEvent final
{
    enum class Kind
    {
        SessionStarted,
        PartialText,
        McpRequest,
        Answer,
        Failure,
        SessionMissing
    };

    Kind kind { Kind::Answer };
    QString text;
    QString detail;
    QString requestId;
    QString serverName;
    QJsonValue message;
};

class StreamJsonParser final
{
public:
    std::vector<StreamEvent> feed(const QByteArray& chunk);
    std::vector<StreamEvent> endOfStream();

    bool isFinished() const;

private:
    void handleLine(const QByteArray& line, std::vector<StreamEvent>& events);
    void handleInit(const QJsonObject& event, std::vector<StreamEvent>& events);
    void handleStreamEvent(const QJsonObject& event, std::vector<StreamEvent>& events);
    void handleControlRequest(const QJsonObject& event, std::vector<StreamEvent>& events);
    void handleResult(const QJsonObject& event, std::vector<StreamEvent>& events);
    void conclude(StreamEvent finalEvent, std::vector<StreamEvent>& events);

    QByteArray m_buffer;
    QString m_messageText;
    bool m_initSeen { false };
    bool m_sessionReported { false };
    bool m_finished { false };
};
