#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QString>
#include <QStringList>
#include <vector>

#include "chat/domain/step.h"

namespace chat::claude
{

struct StreamEvent final
{
    enum class Kind
    {
        SessionStarted,
        StepStarted,
        StepFinished,
        Answer,
        Failure
    };

    Kind kind { Kind::Answer };
    Step step;
    QString text;
    QString detail;
};

class StreamJsonParser final
{
public:
    std::vector<StreamEvent> feed(const QByteArray& chunk);
    std::vector<StreamEvent> endOfStream();

    bool isFinished() const;
    const QStringList& agents() const;

private:
    void handleLine(const QByteArray& line, std::vector<StreamEvent>& events);
    void handleSystem(const QJsonObject& event, std::vector<StreamEvent>& events);
    void handleInit(const QJsonObject& event, std::vector<StreamEvent>& events);
    void handleTaskNotification(const QJsonObject& event, std::vector<StreamEvent>& events);
    void handleAssistant(const QJsonObject& event, std::vector<StreamEvent>& events);
    void handleUser(const QJsonObject& event, std::vector<StreamEvent>& events);
    void handleResult(const QJsonObject& event, std::vector<StreamEvent>& events);

    void startStep(const QJsonObject& toolUse, const QString& parentToolUseId,
                   std::vector<StreamEvent>& events);
    void finishStep(const QString& toolUseId, StepStatus status, const QJsonValue& output,
                    std::vector<StreamEvent>& events);
    void conclude(StreamEvent finalEvent, std::vector<StreamEvent>& events);
    void closeRunningSteps(std::vector<StreamEvent>& events);

    QByteArray m_buffer;
    QHash<QString, Step> m_steps;
    QList<QString> m_startOrder;
    QStringList m_agents;
    bool m_sessionReported { false };
    bool m_finished { false };
};

}
