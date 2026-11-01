#pragma once

namespace chat
{

enum class Role
{
    User,
    Assistant
};

enum class MessageStatus
{
    Running,
    Complete,
    Failed,
    Cancelled,
    Interrupted
};

enum class StepKind
{
    Delegation,
    Tool
};

enum class StepStatus
{
    Running,
    Ok,
    Failed
};

}
