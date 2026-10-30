#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace diagram
{
enum class ClassKind
{
    Class,
    Abstract,
    Struct,
    Enum,
    Interface
};

enum class ClassLinkLine
{
    Solid,
    Dashed
};

enum class ClassLinkEnd
{
    None,
    Triangle,
    FilledDiamond,
    HollowDiamond,
    Arrow
};

enum class ClassNoteSide
{
    Left,
    Right
};

struct ClassType
{
    QString id;
    QString label;
    ClassKind kind = ClassKind::Class;
    QList<QStringList> compartments;
    int line = 0;
};

struct ClassRelation
{
    int from = -1;
    int to = -1;
    ClassLinkLine line = ClassLinkLine::Solid;
    ClassLinkEnd fromEnd = ClassLinkEnd::None;
    ClassLinkEnd toEnd = ClassLinkEnd::None;
    QString fromMultiplicity;
    QString toMultiplicity;
    QString label;
    int sourceLine = 0;
};

struct ClassNote
{
    int type = -1;
    ClassNoteSide side = ClassNoteSide::Right;
    QString text;
};

struct ClassDiagram
{
    QString title;
    QList<ClassType> types;
    QList<ClassRelation> relations;
    QList<ClassNote> notes;
    QStringList warnings;
};
}
