#include "chat/infrastructure/stepserializer.h"

#include "dbtoolkit/sqlcodec.h"

using namespace sql_codec;

namespace chat
{

const QStringList& StepSerializer::columns()
{
    static const QStringList list { kId,     kMessageId, kParentStepId, kPosition, kKind,
                                    kAgent,  kName,      kInput,        kOutput,   kStatus,
                                    kEffect, kStartedAt, kFinishedAt };
    return list;
}

QVariantMap StepSerializer::toRow(const QUuid& messageId, const Step& step, const Timing& timing)
{
    return {
        { kId, toSql(step.id) },
        { kMessageId, toSql(messageId) },
        { kParentStepId, step.parentId ? toSql(*step.parentId) : QVariant() },
        { kPosition, timing.position },
        { kKind, toSql(step.kind) },
        { kAgent, toSql(step.agent) },
        { kName, toSql(step.tool) },
        { kInput, jsonObjectToSql(step.input) },
        { kOutput, jsonValueToSql(step.output) },
        { kStatus, toSql(step.status) },
        { kEffect, toSql(step.effect) },
        { kStartedAt, toSql(timing.startedAt) },
        { kFinishedAt, toSql(timing.finishedAt) },
    };
}

Step StepSerializer::fromRow(const QVariantMap& row)
{
    Step step;
    step.id = uuidFromSql(row.value(kId));
    if (!row.value(kParentStepId).isNull())
    {
        step.parentId = uuidFromSql(row.value(kParentStepId));
    }
    step.kind = enumFromSql<StepKind>(row.value(kKind));
    step.agent = row.value(kAgent).toString();
    step.tool = row.value(kName).toString();
    step.input = jsonObjectFromSql(row.value(kInput));
    step.output = jsonValueFromSql(row.value(kOutput));
    step.status = enumFromSql<StepStatus>(row.value(kStatus));
    step.effect = enumFromSql<ToolEffect>(row.value(kEffect));
    return step;
}

}
