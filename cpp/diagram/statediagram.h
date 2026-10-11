#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace diagram
{
enum class StateKind
{
    Normal,
    Initial,
    Final
};

enum class StateNoteSide
{
    Left,
    Right
};

struct State
{
    QString id;
    QString label;
    QStringList descriptions;
    StateKind kind = StateKind::Normal;
    bool isComposite = false;
    int parent = -1;
    int line = 0;
};

struct StateTransition
{
    int from = -1;
    int to = -1;
    QString label;
    int sourceLine = 0;
};

struct StateNote
{
    int state = -1;
    StateNoteSide side = StateNoteSide::Right;
    QString text;
};

struct StateDiagram
{
    QString title;
    QList<State> states;
    QList<StateTransition> transitions;
    QList<StateNote> notes;
    QStringList warnings;
};
}
