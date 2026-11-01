#include "chat/infrastructure/stepserializer.h"

#include "chat/infrastructure/enumcodec.h"
#include "chat/infrastructure/jsoncodec.h"
#include "dbtoolkit/sqlcodec.h"

using namespace sql_codec;

namespace chat
{

const QStringList& StepSerializer::columns()
{
    static const QStringList list { kId,     kMessageId, kParentStepId, kPosition,
                                    kKind,   kAgent,     kName,         kInput,
                                    kOutput, kStatus,    kStartedAt,    kFinishedAt };
    return list;
}

QVariantMap StepSerializer::toRow(const QUuid& messageId, const Step& step, const Timing& timing)
{
    return {
        { kId, toSql(step.id) },
        { kMessageId, toSql(messageId) },
        { kParentStepId, step.parentId ? toSql(*step.parentId) : QVariant() },
        { kPosition, timing.position },
        { kKind, enum_codec::stepKindToSql(step.kind) },
        { kAgent, toSql(step.agent) },
        { kName, toSql(step.tool) },
        { kInput, json_codec::objectToSql(step.input) },
        { kOutput, json_codec::valueToSql(step.output) },
        { kStatus, enum_codec::stepStatusToSql(step.status) },
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
    step.kind = enum_codec::stepKindFromSql(row.value(kKind).toString());
    step.agent = row.value(kAgent).toString();
    step.tool = row.value(kName).toString();
    step.input = json_codec::objectFromSql(row.value(kInput));
    step.output = json_codec::valueFromSql(row.value(kOutput));
    step.status = enum_codec::stepStatusFromSql(row.value(kStatus).toString());
    return step;
}

}
