#pragma once

#include <QString>

#include "agent/tooleffect.h"
#include "chat/domain/enums.h"

namespace chat
{

inline QString roleName(Role role)
{
    switch (role)
    {
        case Role::User:
            return QStringLiteral("user");
        case Role::Assistant:
            return QStringLiteral("assistant");
    }
    return QString();
}

inline QString statusName(MessageStatus status)
{
    switch (status)
    {
        case MessageStatus::Running:
            return QStringLiteral("running");
        case MessageStatus::Complete:
            return QStringLiteral("complete");
        case MessageStatus::Failed:
            return QStringLiteral("failed");
        case MessageStatus::Cancelled:
            return QStringLiteral("cancelled");
        case MessageStatus::Interrupted:
            return QStringLiteral("interrupted");
    }
    return QString();
}

inline QString stepKindName(StepKind kind)
{
    switch (kind)
    {
        case StepKind::Delegation:
            return QStringLiteral("delegation");
        case StepKind::Tool:
            return QStringLiteral("tool");
    }
    return QString();
}

inline QString stepStatusName(StepStatus status)
{
    switch (status)
    {
        case StepStatus::Running:
            return QStringLiteral("running");
        case StepStatus::Ok:
            return QStringLiteral("ok");
        case StepStatus::Failed:
            return QStringLiteral("failed");
    }
    return QString();
}

inline QString effectName(ToolEffect effect)
{
    switch (effect)
    {
        case ToolEffect::Reads:
            return QStringLiteral("reads");
        case ToolEffect::Writes:
            return QStringLiteral("writes");
    }
    return QString();
}

}
