#include "layoutsupport.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <variant>

namespace diagram::layouting
{
qreal wholePixels(qreal value) { return std::ceil(value - 0.001); }

QList<qreal> fairWidths(const QList<qreal>& minimal, qreal usable)
{
    QList<qreal> widest = minimal;
    std::sort(widest.begin(), widest.end(), std::greater<>());
    qreal remaining = usable;
    qsizetype shared = widest.size();
    for (const qreal width : std::as_const(widest))
    {
        if (width <= remaining / qreal(shared))
            break;
        remaining -= width;
        --shared;
    }
    const qreal share = shared > 0 ? remaining / qreal(shared) : 0;
    QList<qreal> widths;
    for (const qreal width : minimal)
        widths.append(std::max(width, share));
    return widths;
}

TextWrapper::TextWrapper(const DiagramStyle& style, const TextMeasurer& measurer)
    : m_style(style)
    , m_measurer(measurer)
{
}

qreal TextWrapper::measured(const QString& text) const
{
    return m_measurer.width(text) * m_style.textSafety;
}

qreal TextWrapper::widestWord(const QString& text) const
{
    qreal widest = 0;
    for (const QString& paragraph : text.split(QLatin1Char('\n')))
    {
        for (const QString& word : paragraph.split(QLatin1Char(' '), Qt::SkipEmptyParts))
            widest = std::max(widest, measured(word));
    }
    return widest;
}

qreal TextWrapper::minimumWrapWidth() const { return 2 * qreal(m_style.fontPixelSize); }

WrappedText TextWrapper::wrap(const QString& text, qreal maxWidth) const
{
    WrappedText wrapped;
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return wrapped;
    const qreal limit = std::max(maxWidth, minimumWrapWidth());
    for (const QString& paragraph : trimmed.split(QLatin1Char('\n')))
        wrapParagraph(paragraph, limit, wrapped.lines);
    for (const QString& line : std::as_const(wrapped.lines))
        wrapped.width = std::max(wrapped.width, measured(line));
    wrapped.height = qreal(wrapped.lines.size()) * m_measurer.lineHeight();
    return wrapped;
}

void TextWrapper::wrapParagraph(const QString& paragraph, qreal limit, QStringList& lines) const
{
    QString line;
    for (const QString& word : paragraph.split(QLatin1Char(' '), Qt::SkipEmptyParts))
    {
        const QString candidate = line.isEmpty() ? word : line + QLatin1Char(' ') + word;
        if (measured(candidate) <= limit)
        {
            line = candidate;
            continue;
        }
        if (!line.isEmpty())
            lines.append(line);
        line = breakLongWord(word, limit, lines);
    }
    lines.append(line);
}

QString TextWrapper::breakLongWord(const QString& word, qreal limit, QStringList& lines) const
{
    QString piece;
    for (const QChar character : word)
    {
        if (!piece.isEmpty() && measured(piece + character) > limit)
        {
            lines.append(piece);
            piece.clear();
        }
        piece += character;
    }
    return piece;
}

namespace
{
    SceneRect shifted(SceneRect item, qreal dx, qreal dy)
    {
        item.rect.translate(dx, dy);
        return item;
    }

    SceneLine shifted(SceneLine item, qreal dx, qreal dy)
    {
        for (QPointF& point : item.points)
            point += QPointF(dx, dy);
        return item;
    }

    SceneText shifted(SceneText item, qreal dx, qreal dy)
    {
        item.bounds.translate(dx, dy);
        return item;
    }

    SceneEllipse shifted(SceneEllipse item, qreal dx, qreal dy)
    {
        item.rect.translate(dx, dy);
        return item;
    }

    ScenePolygon shifted(ScenePolygon item, qreal dx, qreal dy)
    {
        for (QPointF& point : item.points)
            point += QPointF(dx, dy);
        return item;
    }
}

void appendShifted(QList<SceneItem>& target, const QList<SceneItem>& source, qreal dx, qreal dy)
{
    for (const SceneItem& item : source)
        target.append(std::visit([dx, dy](const auto& typed) -> SceneItem
                                 { return shifted(typed, dx, dy); }, item));
}

QRectF placeFrameTab(QList<SceneItem>& items, const WrappedText& label, const DiagramStyle& style,
                     const QPointF& topLeft)
{
    const qreal width = label.width + 2 * style.padding;
    const qreal height = label.height + style.padding;
    const QRectF tab(topLeft, QSizeF(width, height));
    items.append(SceneRect { tab, 0, ColorRole::BoxFill, ColorRole::GroupStroke, false });
    if (!label.lines.isEmpty())
        items.append(SceneText {
            tab.adjusted(style.padding, style.padding / 2, -style.padding, -style.padding / 2),
            Qt::AlignLeft, label.lines, ColorRole::Text });
    return tab;
}
}
