#include "bitslayout.h"

#include "diagram/layoutsupport.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace diagram
{
namespace
{
    using layouting::WrappedText;

    constexpr qreal kSmallScale = 0.8;
    constexpr qreal kMinBitCellWidth = 12;
    constexpr qreal kMaxBitCellWidth = 40;
    constexpr qreal kMinByteCellWidth = 12;
    constexpr qreal kMaxByteCellWidth = 96;
    constexpr qreal kNumberClearance = 4;
    constexpr qreal kNumberGap = 2;
    constexpr qreal kDescriptionGap = 2;
    constexpr qreal kDescriptionInset = 2;
    constexpr qreal kListGap = 2;
    constexpr qreal kHatchSpacing = 5;
    constexpr int kMostDescriptionLines = 3;

    class ScaledTextMeasurer final : public TextMeasurer
    {
    public:
        ScaledTextMeasurer(const TextMeasurer& base, qreal factor)
            : m_base(base)
            , m_factor(factor)
        {
        }

        qreal width(const QString& text) const override { return m_base.width(text) * m_factor; }
        qreal lineHeight() const override { return m_base.lineHeight() * m_factor; }
        qreal ascent() const override { return m_base.ascent() * m_factor; }

    private:
        const TextMeasurer& m_base;
        qreal m_factor;
    };

    DiagramStyle smallStyleOf(DiagramStyle style)
    {
        style.fontPixelSize = std::max(1, int(std::lround(style.fontPixelSize * kSmallScale)));
        return style;
    }

    enum class PieceKind
    {
        Field,
        Padding,
        Reserved,
        Unknown
    };

    struct Piece
    {
        int span = -1;
        PieceKind kind = PieceKind::Reserved;
        int row = 0;
        int firstColumn = 0;
        int lastColumn = 0;
        int leftUnit = 0;
        int rightUnit = 0;
        bool continuesBefore = false;
        bool continuesAfter = false;
        QString inside;
        bool insideIsMuted = false;
        WrappedText below;
    };

    struct Arrangement
    {
        int columns = 0;
        int rows = 0;
        QList<Piece> pieces;
    };

    struct ListedSpan
    {
        bool label = false;
        bool description = false;
    };

    QList<SceneItem> hatchLines(const QRectF& rect)
    {
        QList<SceneItem> lines;
        for (qreal offset = -rect.height(); offset < rect.width(); offset += kHatchSpacing)
        {
            const qreal from = std::max<qreal>(0, -offset);
            const qreal to = std::min(rect.height(), rect.width() - offset);
            if (to - from < 1)
                continue;
            lines.append(SceneLine { { QPointF(rect.left() + offset + from, rect.bottom() - from),
                                       QPointF(rect.left() + offset + to, rect.bottom() - to) },
                                     ColorRole::Divider,
                                     false,
                                     ArrowHead::None });
        }
        return lines;
    }

    class BitsLayouter
    {
    public:
        BitsLayouter(const BitsDiagram& diagram, const DiagramStyle& style, qreal availableWidth,
                     const TextMeasurer& measurer)
            : m_diagram(diagram)
            , m_style(style)
            , m_measurer(measurer)
            , m_wrapper(style, measurer)
            , m_smallStyle(smallStyleOf(style))
            , m_smallMeasurer(measurer, kSmallScale)
            , m_smallWrapper(m_smallStyle, m_smallMeasurer)
            , m_contentWidth(std::max<qreal>(0, availableWidth - 2 * style.margin))
        {
            indexOwners();
        }

        Scene layout() const
        {
            const bool inBits = m_diagram.mode == BitsMode::Bits;
            const qreal offsetWidth = inBits ? 0 : offsetColumnWidth();
            const int columns = inBits ? bitColumns() : m_diagram.width;
            const qreal cell = inBits ? bitCellWidth(columns) : byteCellWidth(offsetWidth);
            const qreal blockWidth = offsetWidth + qreal(columns) * cell;

            Arrangement arrangement = arrange(columns);
            QList<ListedSpan> listed(m_diagram.spans.size());
            decideTexts(arrangement, cell, listed);

            const WrappedText title = m_wrapper.wrap(m_diagram.title, m_contentWidth);
            const QList<WrappedText> notes = listedNotes(listed, blockWidth);
            qreal totalWidth = std::max(blockWidth, title.width);
            for (const WrappedText& note : notes)
                totalWidth = std::max(totalWidth, note.width);
            const qreal originX = m_style.margin + (totalWidth - blockWidth) / 2;

            Scene scene;
            scene.fontFamily = m_style.fontFamily;
            scene.fontPixelSize = m_style.fontPixelSize;
            scene.lineHeight = m_measurer.lineHeight();
            scene.ascent = m_measurer.ascent();
            scene.arrowHeadSize = m_style.arrowHeadSize;
            scene.size.setWidth(layouting::wholePixels(totalWidth + 2 * m_style.margin));

            qreal y = m_style.margin;
            if (!title.lines.isEmpty())
            {
                scene.items.append(SceneText {
                    QRectF((scene.size.width() - title.width) / 2, y, title.width, title.height),
                    Qt::AlignHCenter, title.lines, ColorRole::Text, 1 });
                y += title.height + m_style.rowGap;
            }
            for (int row = 0; row < arrangement.rows; ++row)
            {
                y = appendRow(scene.items, arrangement, row, originX, offsetWidth, cell, y);
                if (row + 1 < arrangement.rows)
                    y += m_style.rowGap;
            }
            y = appendNotes(scene.items, notes, originX, y);
            scene.size.setHeight(layouting::wholePixels(y + m_style.margin));
            return scene;
        }

    private:
        void indexOwners()
        {
            int units = m_diagram.width;
            if (m_diagram.mode == BitsMode::Bytes)
            {
                int last = -1;
                for (const BitsSpan& span : m_diagram.spans)
                    last = std::max(last, span.high);
                units = (last + m_diagram.width) / m_diagram.width * m_diagram.width;
            }
            m_unitCount = units;
            m_owner = QList<int>(units, -1);
            for (qsizetype index = 0; index < m_diagram.spans.size(); ++index)
            {
                const BitsSpan& span = m_diagram.spans.at(index);
                for (int unit = span.low; unit <= span.high && unit < units; ++unit)
                    m_owner[unit] = int(index);
            }
        }

        qreal numberWidth() const
        {
            return m_smallWrapper.measured(QString::number(std::max(0, m_diagram.width - 1)));
        }

        qreal readableCellWidth() const
        {
            return std::ceil(std::max(kMinBitCellWidth, numberWidth() + kNumberClearance));
        }

        int bitColumns() const
        {
            QList<int> candidates { m_diagram.width };
            for (const int rowBits : { 32, 16, 8 })
            {
                if (rowBits < m_diagram.width)
                    candidates.append(rowBits);
            }
            for (const int columns : std::as_const(candidates))
            {
                if (std::floor(m_contentWidth / columns) >= readableCellWidth())
                    return columns;
            }
            return candidates.last();
        }

        qreal bitCellWidth(int columns) const
        {
            const qreal fitting = std::floor(m_contentWidth / columns);
            return std::min(kMaxBitCellWidth, std::max(readableCellWidth(), fitting));
        }

        qreal offsetColumnWidth() const
        {
            const int rowCount = (m_unitCount + m_diagram.width - 1) / m_diagram.width;
            const int lastOffset = std::max(0, rowCount - 1) * m_diagram.width;
            return std::ceil(m_smallWrapper.measured(QString::number(lastOffset)))
                + m_style.columnGap;
        }

        qreal byteCellWidth(qreal offsetWidth) const
        {
            const qreal fitting = std::floor((m_contentWidth - offsetWidth) / m_diagram.width);
            return std::clamp(fitting, kMinByteCellWidth, kMaxByteCellWidth);
        }

        int spanStart(int span) const
        {
            const BitsSpan& entry = m_diagram.spans.at(span);
            return m_diagram.mode == BitsMode::Bits ? entry.high : entry.low;
        }

        int spanEnd(int span) const
        {
            const BitsSpan& entry = m_diagram.spans.at(span);
            return m_diagram.mode == BitsMode::Bits ? entry.low : entry.high;
        }

        Arrangement arrange(int columns) const
        {
            const bool inBits = m_diagram.mode == BitsMode::Bits;
            Arrangement arrangement;
            arrangement.columns = columns;
            arrangement.rows = (m_unitCount + columns - 1) / columns;
            for (int row = 0; row < arrangement.rows; ++row)
            {
                const int band = inBits ? arrangement.rows - 1 - row : row;
                const int bottom = band * columns;
                const int top = std::min(m_unitCount, bottom + columns) - 1;
                QList<int> units;
                for (int unit = bottom; unit <= top; ++unit)
                    units.append(inBits ? top - (unit - bottom) : unit);
                appendPieces(arrangement, row, units, bottom, columns);
            }
            return arrangement;
        }

        void appendPieces(Arrangement& arrangement, int row, const QList<int>& units, int bottom,
                          int columns) const
        {
            const bool inBits = m_diagram.mode == BitsMode::Bits;
            const auto columnOf
                = [&](int unit) { return inBits ? columns - 1 - (unit - bottom) : unit - bottom; };
            qsizetype begin = 0;
            while (begin < units.size())
            {
                const int owner = m_owner.at(units.at(begin));
                qsizetype end = begin;
                while (end + 1 < units.size() && m_owner.at(units.at(end + 1)) == owner)
                    ++end;
                Piece piece;
                piece.span = owner;
                piece.row = row;
                piece.leftUnit = units.at(begin);
                piece.rightUnit = units.at(end);
                piece.firstColumn = columnOf(piece.leftUnit);
                piece.lastColumn = columnOf(piece.rightUnit);
                if (owner < 0)
                {
                    piece.kind = inBits ? PieceKind::Reserved : PieceKind::Unknown;
                }
                else
                {
                    piece.kind = m_diagram.spans.at(owner).kind == BitsSpanKind::Padding
                        ? PieceKind::Padding
                        : PieceKind::Field;
                    piece.continuesBefore = piece.leftUnit != spanStart(owner);
                    piece.continuesAfter = piece.rightUnit != spanEnd(owner);
                }
                arrangement.pieces.append(piece);
                begin = end + 1;
            }
        }

        static QString inlineText(const QString& text) { return text.simplified(); }

        bool fitsInside(const QString& text, qreal width) const
        {
            return m_wrapper.measured(text) <= width - m_style.padding;
        }

        void decideTexts(Arrangement& arrangement, qreal cell, QList<ListedSpan>& listed) const
        {
            QList<bool> seen(m_diagram.spans.size(), false);
            for (Piece& piece : arrangement.pieces)
            {
                if (piece.kind != PieceKind::Field)
                    continue;
                const BitsSpan& span = m_diagram.spans.at(piece.span);
                const qreal width = qreal(piece.lastColumn - piece.firstColumn + 1) * cell;
                const QString label = inlineText(span.label);
                if (!seen.at(piece.span))
                {
                    seen[piece.span] = true;
                    if (fitsInside(label, width))
                        piece.inside = label;
                    else
                        listed[piece.span].label = true;
                    if (!span.description.isEmpty())
                        decideDescription(piece, span, width, listed[piece.span]);
                    continue;
                }
                piece.insideIsMuted = true;
                for (const QString& candidate :
                     { label + QStringLiteral(" (cont.)"), QStringLiteral("(cont.)") })
                {
                    if (fitsInside(candidate, width))
                    {
                        piece.inside = candidate;
                        break;
                    }
                }
            }
        }

        void decideDescription(Piece& piece, const BitsSpan& span, qreal width,
                               ListedSpan& listed) const
        {
            const qreal room = width - 2 * kDescriptionInset;
            const WrappedText wrapped = m_smallWrapper.wrap(span.description, room);
            const bool fits = m_smallWrapper.widestWord(span.description) <= room
                && wrapped.lines.size() <= kMostDescriptionLines;
            if (fits)
                piece.below = wrapped;
            else
                listed.description = true;
        }

        QString rangeText(const BitsSpan& span) const
        {
            if (m_diagram.mode == BitsMode::Bits)
                return span.low == span.high ? QString::number(span.low)
                                             : QStringLiteral("%1:%2").arg(span.high).arg(span.low);
            return span.low == span.high ? QString::number(span.low)
                                         : QStringLiteral("%1..%2").arg(span.low).arg(span.high);
        }

        QList<qsizetype> spansInReadingOrder() const
        {
            QList<qsizetype> order;
            for (qsizetype index = 0; index < m_diagram.spans.size(); ++index)
                order.append(index);
            const bool inBits = m_diagram.mode == BitsMode::Bits;
            std::stable_sort(order.begin(), order.end(),
                             [&](qsizetype a, qsizetype b)
                             {
                                 const BitsSpan& first = m_diagram.spans.at(a);
                                 const BitsSpan& second = m_diagram.spans.at(b);
                                 return inBits ? first.high > second.high : first.low < second.low;
                             });
            return order;
        }

        QList<WrappedText> listedNotes(const QList<ListedSpan>& listed, qreal blockWidth) const
        {
            QList<WrappedText> notes;
            for (const qsizetype index : spansInReadingOrder())
            {
                const ListedSpan& entry = listed.at(index);
                if (!entry.label && !entry.description)
                    continue;
                const BitsSpan& span = m_diagram.spans.at(index);
                QString text
                    = QStringLiteral("[%1] %2").arg(rangeText(span), inlineText(span.label));
                if (entry.description)
                    text += QStringLiteral(" - ") + inlineText(span.description);
                notes.append(m_smallWrapper.wrap(text, blockWidth));
            }
            return notes;
        }

        qreal appendNotes(QList<SceneItem>& items, const QList<WrappedText>& notes, qreal originX,
                          qreal y) const
        {
            if (notes.isEmpty())
                return y;
            y += m_style.rowGap;
            for (qsizetype index = 0; index < notes.size(); ++index)
            {
                const WrappedText& note = notes.at(index);
                items.append(SceneText { QRectF(originX, y, note.width, note.height), Qt::AlignLeft,
                                         note.lines, ColorRole::Text, kSmallScale });
                y += note.height;
                if (index + 1 < notes.size())
                    y += kListGap;
            }
            return y;
        }

        qreal appendRow(QList<SceneItem>& items, const Arrangement& arrangement, int row,
                        qreal originX, qreal offsetWidth, qreal cell, qreal top) const
        {
            const bool inBits = m_diagram.mode == BitsMode::Bits;
            const qreal smallHeight = m_smallMeasurer.lineHeight();
            const qreal boxHeight = m_measurer.lineHeight() + 2 * m_style.padding;
            const qreal gridLeft = originX + offsetWidth;

            qreal boxTop = top;
            if (inBits)
                boxTop += smallHeight + kNumberGap;
            if (!inBits)
            {
                const QString offset = QString::number(row * m_diagram.width);
                const qreal width = m_smallWrapper.measured(offset);
                items.append(
                    SceneText { QRectF(originX + offsetWidth - m_style.columnGap - width,
                                       boxTop + (boxHeight - smallHeight) / 2, width, smallHeight),
                                Qt::AlignRight,
                                { offset },
                                ColorRole::MutedText,
                                kSmallScale });
            }

            qreal belowHeight = 0;
            for (const Piece& piece : arrangement.pieces)
            {
                if (piece.row != row)
                    continue;
                const QRectF box(gridLeft + qreal(piece.firstColumn) * cell, boxTop,
                                 qreal(piece.lastColumn - piece.firstColumn + 1) * cell, boxHeight);
                appendBox(items, piece, box);
                if (inBits)
                    appendNumbers(items, piece, gridLeft, cell, top);
                if (!piece.below.lines.isEmpty())
                {
                    items.append(SceneText { QRectF(box.center().x() - piece.below.width / 2,
                                                    box.bottom() + kDescriptionGap,
                                                    piece.below.width, piece.below.height),
                                             Qt::AlignHCenter, piece.below.lines,
                                             ColorRole::MutedText, kSmallScale });
                    belowHeight = std::max(belowHeight, kDescriptionGap + piece.below.height);
                }
            }
            return boxTop + boxHeight + belowHeight;
        }

        void appendNumbers(QList<SceneItem>& items, const Piece& piece, qreal gridLeft, qreal cell,
                           qreal top) const
        {
            const qreal height = m_smallMeasurer.lineHeight();
            const auto number = [&](int unit, int column)
            {
                const QString text = QString::number(unit);
                const qreal width = m_smallWrapper.measured(text);
                const qreal centre = gridLeft + (qreal(column) + 0.5) * cell;
                items.append(SceneText { QRectF(centre - width / 2, top, width, height),
                                         Qt::AlignHCenter,
                                         { text },
                                         ColorRole::MutedText,
                                         kSmallScale });
            };
            number(piece.leftUnit, piece.firstColumn);
            if (piece.lastColumn != piece.firstColumn)
                number(piece.rightUnit, piece.lastColumn);
        }

        void appendBox(QList<SceneItem>& items, const Piece& piece, const QRectF& box) const
        {
            switch (piece.kind)
            {
                case PieceKind::Field:
                    appendFieldBox(items, piece, box);
                    return;
                case PieceKind::Padding:
                    items.append(
                        SceneRect { box, 0, ColorRole::Background, ColorRole::BoxStroke, false });
                    break;
                case PieceKind::Reserved:
                    items.append(
                        SceneRect { box, 0, ColorRole::Background, ColorRole::Divider, false });
                    break;
                case PieceKind::Unknown:
                    items.append(SceneRect { box, 0, std::nullopt, ColorRole::Divider, true });
                    return;
            }
            items.append(hatchLines(box));
        }

        void appendFieldBox(QList<SceneItem>& items, const Piece& piece, const QRectF& box) const
        {
            if (!piece.continuesBefore && !piece.continuesAfter)
                items.append(SceneRect { box, 0, ColorRole::BoxFill, ColorRole::BoxStroke, false });
            else
                appendContinuedBox(items, piece, box);
            if (piece.inside.isEmpty())
                return;
            const qreal width = m_wrapper.measured(piece.inside);
            items.append(
                SceneText { QRectF(box.center().x() - width / 2,
                                   box.top() + (box.height() - m_measurer.lineHeight()) / 2, width,
                                   m_measurer.lineHeight()),
                            Qt::AlignHCenter,
                            { piece.inside },
                            piece.insideIsMuted ? ColorRole::MutedText : ColorRole::Text,
                            1 });
        }

        static void appendContinuedBox(QList<SceneItem>& items, const Piece& piece,
                                       const QRectF& box)
        {
            items.append(SceneRect { box, 0, ColorRole::BoxFill, std::nullopt, false });
            const auto edge = [&](const QPointF& from, const QPointF& to, bool dashed) {
                items.append(
                    SceneLine { { from, to }, ColorRole::BoxStroke, dashed, ArrowHead::None });
            };
            edge(box.topLeft(), box.topRight(), false);
            edge(box.bottomLeft(), box.bottomRight(), false);
            edge(box.topLeft(), box.bottomLeft(), piece.continuesBefore);
            edge(box.topRight(), box.bottomRight(), piece.continuesAfter);
        }

        const BitsDiagram& m_diagram;
        const DiagramStyle& m_style;
        const TextMeasurer& m_measurer;
        layouting::TextWrapper m_wrapper;
        DiagramStyle m_smallStyle;
        ScaledTextMeasurer m_smallMeasurer;
        layouting::TextWrapper m_smallWrapper;
        qreal m_contentWidth;
        int m_unitCount = 0;
        QList<int> m_owner;
    };
}

Scene BitsLayout::layout(const BitsDiagram& diagram, const DiagramStyle& style,
                         qreal availableWidth, const TextMeasurer& measurer)
{
    return BitsLayouter(diagram, style, availableWidth, measurer).layout();
}
}
