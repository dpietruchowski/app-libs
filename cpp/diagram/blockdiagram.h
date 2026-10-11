#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace diagram
{
enum class BlockKind
{
    Rectangle,
    Node,
    Component,
    Database
};

enum class BlockLinkLine
{
    Solid,
    Dotted
};

enum class BlockLinkHead
{
    None,
    ToTarget,
    ToSource
};

enum class BlockNoteSide
{
    Left,
    Right
};

struct Block
{
    QString id;
    QString label;
    BlockKind kind = BlockKind::Rectangle;
    bool isContainer = false;
    int parent = -1;
    int line = 0;
};

struct BlockLink
{
    int from = -1;
    int to = -1;
    QString label;
    BlockLinkLine line = BlockLinkLine::Solid;
    BlockLinkHead head = BlockLinkHead::None;
    int sourceLine = 0;
};

struct BlockNote
{
    int block = -1;
    BlockNoteSide side = BlockNoteSide::Right;
    QString text;
};

struct BlockDiagram
{
    QString title;
    QList<Block> blocks;
    QList<BlockLink> links;
    QList<BlockNote> notes;
    QStringList warnings;
};
}
