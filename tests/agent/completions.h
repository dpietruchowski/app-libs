#pragma once

#include <QList>
#include <QString>
#include <QVariantMap>

#include "agent/structures.h"

namespace agent_test
{

inline Completion contentCompletion(const QString& text)
{
    Completion completion;
    Choice choice;
    choice.index = 0;
    choice.finish_reason = "stop";
    choice.message = Message { .role = "assistant", .content = text };
    completion.choices.append(choice);
    return completion;
}

inline ToolCall toolCall(const QString& id, const QString& name, const QVariantMap& arguments)
{
    ToolCall call;
    call.id = id;
    call.name = name;
    call.type = "function";
    call.arguments = arguments;
    return call;
}

inline Completion toolCallsCompletion(const QList<ToolCall>& calls)
{
    Completion completion;
    Choice choice;
    choice.index = 0;
    choice.finish_reason = "tool_calls";
    Message message { .role = "assistant", .content = "" };
    for (const ToolCall& call : calls)
    {
        message.tool_calls.append(call);
    }
    choice.message = message;
    completion.choices.append(choice);
    return completion;
}

inline Completion toolCallCompletion(const QString& id, const QString& name,
                                     const QVariantMap& arguments)
{
    return toolCallsCompletion({ toolCall(id, name, arguments) });
}

inline Completion failedCompletion(const QString& errorToken)
{
    Completion completion;
    completion.error = errorToken;
    return completion;
}

}
