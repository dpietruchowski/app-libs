#pragma once

#include <QList>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QStringList>

#include <optional>
#include <variant>

namespace diagram
{
enum class ColorRole
{
    Background,
    Line,
    Text,
    MutedText,
    BoxFill,
    BoxStroke,
    NoteFill,
    NoteStroke,
    GroupStroke,
    Divider
};

enum class ArrowHead
{
    None,
    Filled,
    Open
};

struct SceneRect
{
    QRectF rect;
    qreal radius = 0;
    std::optional<ColorRole> fill;
    std::optional<ColorRole> stroke;
    bool dashed = false;
};

struct SceneLine
{
    QList<QPointF> points;
    ColorRole color = ColorRole::Line;
    bool dashed = false;
    ArrowHead head = ArrowHead::None;
};

struct SceneText
{
    QRectF bounds;
    Qt::Alignment align = Qt::AlignHCenter;
    QStringList lines;
    ColorRole color = ColorRole::Text;
};

using SceneItem = std::variant<SceneRect, SceneLine, SceneText>;

struct Scene
{
    QSizeF size;
    QString fontFamily;
    qreal fontPixelSize = 0;
    qreal lineHeight = 0;
    qreal ascent = 0;
    qreal arrowHeadSize = 0;
    QList<SceneItem> items;
};
}
