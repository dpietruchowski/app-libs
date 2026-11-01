#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QVariantMap>

#include "chat/domain/step.h"

namespace chat
{

class StepSerializer final
{
public:
    static inline const QString kTable = QStringLiteral("chat_step");
    static inline const QString kId = QStringLiteral("id");
    static inline const QString kMessageId = QStringLiteral("message_id");
    static inline const QString kParentStepId = QStringLiteral("parent_step_id");
    static inline const QString kPosition = QStringLiteral("position");
    static inline const QString kKind = QStringLiteral("kind");
    static inline const QString kAgent = QStringLiteral("agent");
    static inline const QString kName = QStringLiteral("name");
    static inline const QString kInput = QStringLiteral("input_json");
    static inline const QString kOutput = QStringLiteral("output_json");
    static inline const QString kStatus = QStringLiteral("status");
    static inline const QString kStartedAt = QStringLiteral("started_at");
    static inline const QString kFinishedAt = QStringLiteral("finished_at");

    struct Timing final
    {
        int position { 0 };
        QDateTime startedAt;
        QDateTime finishedAt;
    };

    static const QStringList& columns();

    static QVariantMap toRow(const QUuid& messageId, const Step& step, const Timing& timing);
    static Step fromRow(const QVariantMap& row);
};

}
