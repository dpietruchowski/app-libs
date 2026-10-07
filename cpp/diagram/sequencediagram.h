#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <variant>

namespace diagram
{
enum class ParticipantKind
{
    Box,
    Actor
};

enum class LineStyle
{
    Solid,
    Dashed
};

enum class MessageHead
{
    Filled,
    Open
};

enum class NotePlacement
{
    Left,
    Right,
    Over
};

struct Participant
{
    QString id;
    QString label;
    ParticipantKind kind = ParticipantKind::Box;

    bool operator==(const Participant&) const = default;
};

struct Message
{
    int from = 0;
    int to = 0;
    QString label;
    LineStyle line = LineStyle::Solid;
    MessageHead head = MessageHead::Filled;

    bool operator==(const Message&) const = default;
};

struct Note
{
    int first = 0;
    int last = 0;
    NotePlacement placement = NotePlacement::Right;
    QString text;

    bool operator==(const Note&) const = default;
};

struct Divider
{
    QString label;

    bool operator==(const Divider&) const = default;
};

struct GroupStart
{
    QString keyword;
    QString label;

    bool operator==(const GroupStart&) const = default;
};

struct GroupElse
{
    QString label;

    bool operator==(const GroupElse&) const = default;
};

struct GroupEnd
{
    bool operator==(const GroupEnd&) const = default;
};

using SequenceStep = std::variant<Message, Note, Divider, GroupStart, GroupElse, GroupEnd>;

struct SequenceDiagram
{
    QString title;
    QList<Participant> participants;
    QList<SequenceStep> steps;
    QStringList warnings;
};
}
