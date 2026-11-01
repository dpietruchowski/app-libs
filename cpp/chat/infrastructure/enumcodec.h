#pragma once

#include <QString>

#include "chat/domain/enums.h"

namespace chat::enum_codec
{

QString roleToSql(Role role);
Role roleFromSql(const QString& name);

QString messageStatusToSql(MessageStatus status);
MessageStatus messageStatusFromSql(const QString& name);

QString stepKindToSql(StepKind kind);
StepKind stepKindFromSql(const QString& name);

QString stepStatusToSql(StepStatus status);
StepStatus stepStatusFromSql(const QString& name);

}
