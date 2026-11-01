#include "chat/infrastructure/enumcodec.h"

#include <QLatin1String>
#include <cstddef>

namespace chat::enum_codec
{

namespace
{

    template <typename E> struct Entry final
    {
        E value;
        const char* name;
    };

    template <typename E, std::size_t N> QString nameOf(const Entry<E> (&entries)[N], E value)
    {
        for (const Entry<E>& entry : entries)
        {
            if (entry.value == value)
            {
                return QString::fromLatin1(entry.name);
            }
        }
        return QString();
    }

    template <typename E, std::size_t N>
    E valueOf(const Entry<E> (&entries)[N], const QString& name, E fallback)
    {
        for (const Entry<E>& entry : entries)
        {
            if (name == QLatin1String(entry.name))
            {
                return entry.value;
            }
        }
        return fallback;
    }

    constexpr Entry<Role> kRoles[] = {
        { Role::User, "user" },
        { Role::Assistant, "assistant" },
    };

    constexpr Entry<MessageStatus> kMessageStatuses[] = {
        { MessageStatus::Running, "running" },         { MessageStatus::Complete, "complete" },
        { MessageStatus::Failed, "failed" },           { MessageStatus::Cancelled, "cancelled" },
        { MessageStatus::Interrupted, "interrupted" },
    };

    constexpr Entry<StepKind> kStepKinds[] = {
        { StepKind::Delegation, "delegation" },
        { StepKind::Tool, "tool" },
    };

    constexpr Entry<StepStatus> kStepStatuses[] = {
        { StepStatus::Running, "running" },
        { StepStatus::Ok, "ok" },
        { StepStatus::Failed, "failed" },
    };

}

QString roleToSql(Role role) { return nameOf(kRoles, role); }

Role roleFromSql(const QString& name) { return valueOf(kRoles, name, Role::User); }

QString messageStatusToSql(MessageStatus status) { return nameOf(kMessageStatuses, status); }

MessageStatus messageStatusFromSql(const QString& name)
{
    return valueOf(kMessageStatuses, name, MessageStatus::Interrupted);
}

QString stepKindToSql(StepKind kind) { return nameOf(kStepKinds, kind); }

StepKind stepKindFromSql(const QString& name) { return valueOf(kStepKinds, name, StepKind::Tool); }

QString stepStatusToSql(StepStatus status) { return nameOf(kStepStatuses, status); }

StepStatus stepStatusFromSql(const QString& name)
{
    return valueOf(kStepStatuses, name, StepStatus::Failed);
}

}
