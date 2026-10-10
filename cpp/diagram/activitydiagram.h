#pragma once

#include <QString>
#include <QStringList>

#include <optional>
#include <variant>
#include <vector>

namespace diagram
{
enum class FlowEnd
{
    Stop,
    End
};

enum class BranchEndKind
{
    Detach,
    Kill
};

enum class ActivityNoteSide
{
    Left,
    Right
};

struct ActivityElement;
using ActivityBlock = std::vector<ActivityElement>;

struct ActivityNote
{
    ActivityNoteSide side = ActivityNoteSide::Right;
    QString text;
};

struct ActivityStart
{
};

struct ActivityStop
{
    FlowEnd kind = FlowEnd::Stop;
};

struct ActivityAction
{
    QString text;
    std::optional<ActivityNote> note;
};

struct ActivityBranchEnd
{
    BranchEndKind kind = BranchEndKind::Detach;
};

struct ActivityIfBranch
{
    QString condition;
    QString guard;
    bool isElse = false;
    ActivityBlock block;
};

struct ActivityIf
{
    std::vector<ActivityIfBranch> branches;
};

struct ActivityWhile
{
    QString condition;
    QString yesLabel;
    QString exitLabel;
    ActivityBlock block;
};

struct ActivityRepeat
{
    ActivityBlock block;
    QString condition;
    QString yesLabel;
    QString exitLabel;
};

struct ActivityFork
{
    std::vector<ActivityBlock> branches;
};

struct ActivityPartition
{
    QString label;
    ActivityBlock block;
};

using ActivityNode
    = std::variant<ActivityStart, ActivityStop, ActivityAction, ActivityBranchEnd, ActivityIf,
                   ActivityWhile, ActivityRepeat, ActivityFork, ActivityPartition>;

struct ActivityElement
{
    ActivityNode node;
    QString incomingLabel;
    int line = 0;
};

struct ActivityDiagram
{
    QString title;
    ActivityBlock body;
    QStringList warnings;
};
}
