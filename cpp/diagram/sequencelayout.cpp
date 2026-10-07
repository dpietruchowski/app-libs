#include "sequencelayout.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace diagram
{
namespace
{
    struct WrappedText
    {
        QStringList lines;
        qreal width = 0;
        qreal height = 0;
    };

    struct OpenGroup
    {
        qreal top = 0;
        qreal left = 0;
        qreal right = 0;
    };

    struct NoteSpot
    {
        qreal left = 0;
        qreal width = 0;
        qreal centre = 0;
        NotePlacement placement = NotePlacement::Over;
    };

    QString bracketed(const QString& label)
    {
        return label.isEmpty() ? QString() : QLatin1Char('[') + label + QLatin1Char(']');
    }

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

    class SequenceLayouter
    {
    public:
        SequenceLayouter(const SequenceDiagram& diagram, const DiagramStyle& style,
                         qreal availableWidth, const TextMeasurer& measurer)
            : m_diagram(diagram)
            , m_style(style)
            , m_measurer(measurer)
            , m_availableWidth(availableWidth)
        {
        }

        Scene layout()
        {
            placeColumns();
            m_y = m_style.margin;
            placeTitle();

            const qreal headTop = m_y;
            const qreal headHeight = participantRowHeight();
            placeParticipants(headTop, headHeight, true);
            m_y = headTop + headHeight;
            const qreal lifelineTop = m_y;

            for (const SequenceStep& step : m_diagram.steps)
                std::visit([this](const auto& item) { place(item); }, step);

            m_y += m_style.rowGap;
            const qreal lifelineBottom = m_y;
            if (m_diagram.steps.size() > m_style.repeatFootAfterSteps)
            {
                placeParticipants(lifelineBottom, headHeight, false);
                m_y += headHeight;
            }
            placeLifelines(lifelineTop, lifelineBottom);

            Scene scene;
            scene.size = QSizeF(m_sceneWidth, wholePixels(m_y + m_style.margin));
            scene.fontFamily = m_style.fontFamily;
            scene.fontPixelSize = m_style.fontPixelSize;
            scene.lineHeight = m_measurer.lineHeight();
            scene.ascent = m_measurer.ascent();
            scene.arrowHeadSize = m_style.arrowHeadSize;
            scene.items = m_lifelines + m_frames + m_items;
            return scene;
        }

    private:
        void placeColumns()
        {
            const qsizetype count = std::max<qsizetype>(m_diagram.participants.size(), 1);
            QList<qreal> minimal;
            for (qsizetype index = 0; index < count; ++index)
                minimal.append(minimalColumnWidth(index));
            m_columnWidths = fairWidths(minimal, m_availableWidth - 2 * m_style.margin);
            qreal left = m_style.margin;
            for (const qreal width : std::as_const(m_columnWidths))
            {
                m_columnLefts.append(left);
                left += width;
            }
            m_sceneWidth = wholePixels(left + m_style.margin);
        }

        qreal minimalColumnWidth(qsizetype index) const
        {
            if (index >= m_diagram.participants.size())
                return m_style.minColumnWidth;
            const Participant& participant = m_diagram.participants.at(index);
            const qreal word = widestWord(participant.label);
            const qreal body = participant.kind == ParticipantKind::Actor
                ? std::max(word, actorWidth())
                : word + 2 * m_style.padding;
            return std::clamp(body + m_style.columnGap, m_style.minColumnWidth,
                              std::max(m_style.minColumnWidth, m_style.minLabelWidth));
        }

        qreal actorWidth() const { return m_style.actorHeight * 0.6; }

        qreal widestWord(const QString& text) const
        {
            qreal widest = 0;
            for (const QString& paragraph : text.split(QLatin1Char('\n')))
            {
                for (const QString& word : paragraph.split(QLatin1Char(' '), Qt::SkipEmptyParts))
                    widest = std::max(widest, measured(word));
            }
            return widest;
        }

        qreal lifelineX(int index) const
        {
            return m_columnLefts.at(index) + m_columnWidths.at(index) / 2;
        }

        qreal contentRight() const { return m_sceneWidth - m_style.margin; }

        qreal contentWidth() const { return contentRight() - m_style.margin; }

        qreal readableWidth(qreal width) const
        {
            return std::min(contentWidth(),
                            std::max({ width, m_style.minLabelWidth, contentWidth() / 2 }));
        }

        qreal clampedLeft(qreal left, qreal width) const
        {
            return std::max(m_style.margin, std::min(left, contentRight() - width));
        }

        qreal minimumWrapWidth() const { return 2 * qreal(m_style.fontPixelSize); }

        qreal measured(const QString& text) const
        {
            return m_measurer.width(text) * m_style.textSafety;
        }

        WrappedText wrap(const QString& text, qreal maxWidth) const
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

        void wrapParagraph(const QString& paragraph, qreal limit, QStringList& lines) const
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

        QString breakLongWord(const QString& word, qreal limit, QStringList& lines) const
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

        void addText(const WrappedText& text, const QRectF& bounds, Qt::Alignment align,
                     ColorRole color = ColorRole::Text)
        {
            if (!text.lines.isEmpty())
                m_items.append(SceneText { bounds, align, text.lines, color });
        }

        void placeTitle()
        {
            const WrappedText title = wrap(m_diagram.title, contentWidth());
            if (title.lines.isEmpty())
                return;
            const QRectF bounds((m_sceneWidth - title.width) / 2, m_y, title.width, title.height);
            addText(title, bounds, Qt::AlignHCenter);
            m_y += title.height + m_style.rowGap;
        }

        WrappedText participantLabel(qsizetype index) const
        {
            const Participant& participant = m_diagram.participants.at(index);
            const qreal inner
                = participant.kind == ParticipantKind::Actor ? 0 : 2 * m_style.padding;
            return wrap(participant.label, m_columnWidths.at(index) - m_style.columnGap - inner);
        }

        qreal participantHeight(qsizetype index) const
        {
            const WrappedText label = participantLabel(index);
            if (m_diagram.participants.at(index).kind == ParticipantKind::Actor)
                return m_style.actorHeight + label.height;
            return label.height + 2 * m_style.padding;
        }

        qreal participantRowHeight() const
        {
            qreal height = 0;
            for (qsizetype index = 0; index < m_diagram.participants.size(); ++index)
                height = std::max(height, participantHeight(index));
            return height;
        }

        void placeParticipants(qreal rowTop, qreal rowHeight, bool alignToBottom)
        {
            for (qsizetype index = 0; index < m_diagram.participants.size(); ++index)
            {
                const qreal x = lifelineX(int(index));
                const WrappedText label = participantLabel(index);
                const qreal top
                    = alignToBottom ? rowTop + rowHeight - participantHeight(index) : rowTop;
                if (m_diagram.participants.at(index).kind == ParticipantKind::Actor)
                {
                    placeActor(x, top);
                    addText(label,
                            QRectF(x - label.width / 2, top + m_style.actorHeight, label.width,
                                   label.height),
                            Qt::AlignHCenter);
                    continue;
                }
                const qreal width = label.width + 2 * m_style.padding;
                const QRectF box(x - width / 2, top, width, label.height + 2 * m_style.padding);
                m_items.append(SceneRect { box, m_style.boxRadius, ColorRole::BoxFill,
                                           ColorRole::BoxStroke, false });
                addText(label,
                        box.adjusted(m_style.padding, m_style.padding, -m_style.padding,
                                     -m_style.padding),
                        Qt::AlignHCenter);
            }
        }

        void placeActor(qreal x, qreal top)
        {
            const qreal head = m_style.actorHeight * 0.3;
            const qreal halfWidth = actorWidth() / 2;
            const qreal neck = top + head;
            const qreal hip = top + m_style.actorHeight * 0.68;
            const qreal feet = top + m_style.actorHeight - m_style.padding / 2;
            m_items.append(SceneRect { QRectF(x - head / 2, top, head, head), head / 2,
                                       ColorRole::BoxFill, ColorRole::BoxStroke, false });
            const auto stroke = [this](QList<QPointF> points) {
                m_items.append(
                    SceneLine { std::move(points), ColorRole::BoxStroke, false, ArrowHead::None });
            };
            stroke({ QPointF(x, neck), QPointF(x, hip) });
            stroke({ QPointF(x - halfWidth, neck + head * 0.5),
                     QPointF(x + halfWidth, neck + head * 0.5) });
            stroke({ QPointF(x - halfWidth, feet), QPointF(x, hip), QPointF(x + halfWidth, feet) });
        }

        void placeLifelines(qreal top, qreal bottom)
        {
            for (qsizetype index = 0; index < m_diagram.participants.size(); ++index)
            {
                const qreal x = lifelineX(int(index));
                m_lifelines.append(SceneLine { { QPointF(x, top), QPointF(x, bottom) },
                                               ColorRole::Line,
                                               true,
                                               ArrowHead::None });
            }
        }

        static ArrowHead arrowHead(MessageHead head)
        {
            return head == MessageHead::Open ? ArrowHead::Open : ArrowHead::Filled;
        }

        void addLabel(const WrappedText& label, const QRectF& bounds, Qt::Alignment align,
                      ColorRole color = ColorRole::Text)
        {
            if (label.lines.isEmpty())
                return;
            const QRectF halo = bounds.adjusted(-m_style.labelHalo, 0, m_style.labelHalo, 0);
            m_items.append(SceneRect { halo, 0, ColorRole::Background, std::nullopt, false });
            addText(label, bounds, align, color);
        }

        void place(const Message& message)
        {
            if (message.from == message.to)
            {
                placeSelfMessage(message);
                return;
            }
            m_y += m_style.rowGap;
            const qreal fromX = lifelineX(message.from);
            const qreal toX = lifelineX(message.to);
            const WrappedText label
                = wrap(message.label, readableWidth(std::abs(toX - fromX) - 2 * m_style.padding));
            const QRectF bounds(clampedLeft((fromX + toX - label.width) / 2, label.width), m_y,
                                label.width, label.height);
            m_y += label.height + m_style.arrowHeadSize / 2;
            m_items.append(SceneLine { { QPointF(fromX, m_y), QPointF(toX, m_y) },
                                       ColorRole::Line,
                                       message.line == LineStyle::Dashed,
                                       arrowHead(message.head) });
            addLabel(label, bounds, Qt::AlignHCenter);
            m_y += m_style.arrowHeadSize / 2;
        }

        void placeSelfMessage(const Message& message)
        {
            m_y += m_style.rowGap;
            const qreal x = lifelineX(message.from);
            const qreal reach = m_style.selfLoopWidth + m_style.padding;
            const qreal rightRoom = contentRight() - x - reach;
            const qreal leftRoom = x - reach - m_style.margin;
            const bool toRight = rightRoom >= m_style.minLabelWidth || rightRoom >= leftRoom;
            const qreal loopEdge = toRight ? x + m_style.selfLoopWidth : x - m_style.selfLoopWidth;
            const WrappedText label = wrap(message.label, toRight ? rightRoom : leftRoom);
            const qreal loopHeight = std::max(label.height, m_measurer.lineHeight());
            const qreal top = m_y;
            const qreal bottom = top + loopHeight;
            m_items.append(SceneLine { { QPointF(x, top), QPointF(loopEdge, top),
                                         QPointF(loopEdge, bottom), QPointF(x, bottom) },
                                       ColorRole::Line,
                                       message.line == LineStyle::Dashed,
                                       arrowHead(message.head) });
            const qreal labelLeft
                = toRight ? loopEdge + m_style.padding : loopEdge - m_style.padding - label.width;
            addLabel(label, QRectF(labelLeft, top, label.width, label.height),
                     toRight ? Qt::AlignLeft : Qt::AlignRight);
            m_y = bottom + m_style.arrowHeadSize / 2;
        }

        std::optional<NoteSpot> sideSpot(const Note& note) const
        {
            const qreal halfGap = m_style.columnGap / 2;
            const qreal x = lifelineX(note.first);
            const bool right = note.placement == NotePlacement::Right;
            const int neighbour = right ? note.first + 1 : note.first - 1;
            const qreal neighbourRoom = neighbour >= 0 && neighbour < m_diagram.participants.size()
                ? std::abs(lifelineX(neighbour) - x) - 2 * halfGap
                : 0;
            const qreal edgeRoom
                = right ? contentRight() - x - halfGap : x - halfGap - m_style.margin;
            const qreal width = std::min(edgeRoom, std::max(neighbourRoom, m_style.minLabelWidth));
            const bool roomy = width >= m_style.minLabelWidth
                || wrap(note.text, width - 2 * m_style.padding).lines.size()
                    == note.text.trimmed().count(QLatin1Char('\n')) + 1;
            if (width < m_style.minColumnWidth || !roomy)
                return std::nullopt;
            const qreal left = right ? x + halfGap : x - halfGap - width;
            return NoteSpot { left, width, left + width / 2, note.placement };
        }

        NoteSpot noteSpot(const Note& note) const
        {
            if (note.placement != NotePlacement::Over)
            {
                if (const auto side = sideSpot(note))
                    return *side;
            }
            const qreal halfGap = m_style.columnGap / 2;
            const qreal left = m_columnLefts.at(note.first) + halfGap;
            const qreal right
                = m_columnLefts.at(note.last) + m_columnWidths.at(note.last) - halfGap;
            const qreal width = readableWidth(right - left);
            return NoteSpot { clampedLeft((left + right - width) / 2, width), width,
                              (left + right) / 2, NotePlacement::Over };
        }

        void place(const Note& note)
        {
            m_y += m_style.rowGap;
            const NoteSpot spot = noteSpot(note);
            const WrappedText text = wrap(note.text, spot.width - 2 * m_style.padding);
            const qreal width = std::min(spot.width, text.width + 2 * m_style.padding);
            const qreal left = spot.placement == NotePlacement::Left
                ? spot.left + spot.width - width
                : spot.placement == NotePlacement::Right
                ? spot.left
                : clampedLeft(spot.centre - width / 2, width);
            const QRectF box(left, m_y, width, text.height + 2 * m_style.padding);
            m_items.append(SceneRect { box, 0, ColorRole::NoteFill, ColorRole::NoteStroke, false });
            addText(
                text,
                box.adjusted(m_style.padding, m_style.padding, -m_style.padding, -m_style.padding),
                Qt::AlignLeft);
            m_y = box.bottom();
        }

        void place(const Divider& divider)
        {
            m_y += m_style.rowGap;
            const WrappedText label = wrap(divider.label, contentWidth() - 4 * m_style.padding);
            const qreal height
                = label.lines.isEmpty() ? 2 * m_style.padding : label.height + m_style.padding;
            const qreal middle = m_y + height / 2;
            for (const qreal offset : { -1.5, 1.5 })
            {
                m_items.append(SceneLine { { QPointF(m_style.margin, middle + offset),
                                             QPointF(contentRight(), middle + offset) },
                                           ColorRole::Divider,
                                           false,
                                           ArrowHead::None });
            }
            if (!label.lines.isEmpty())
            {
                const qreal width = label.width + 2 * m_style.padding;
                const QRectF box((m_sceneWidth - width) / 2, m_y, width, height);
                m_items.append(
                    SceneRect { box, 0, ColorRole::Background, ColorRole::Divider, false });
                addText(label,
                        box.adjusted(m_style.padding, m_style.padding / 2, -m_style.padding,
                                     -m_style.padding / 2),
                        Qt::AlignHCenter);
            }
            m_y += height;
        }

        qsizetype groupIndent() const
        {
            const qreal spare = contentWidth() - m_style.minLabelWidth / 2;
            const auto fitting = qsizetype(std::max(0.0, spare / (2 * m_style.groupInset)));
            return std::min({ m_groups.size(), qsizetype(m_style.maxGroupIndent), fitting });
        }

        void place(const GroupStart& group)
        {
            m_y += m_style.rowGap;
            const qreal inset = m_style.margin + qreal(groupIndent()) * m_style.groupInset;
            const OpenGroup frame { m_y, inset, m_sceneWidth - inset };
            m_groups.append(frame);

            const qreal frameWidth = frame.right - frame.left;
            const WrappedText keyword = wrap(group.keyword, frameWidth - 2 * m_style.padding);
            const qreal tabWidth = keyword.width + 2 * m_style.padding;
            const qreal tabHeight = keyword.height + m_style.padding;
            const QRectF tab(frame.left, frame.top, tabWidth, tabHeight);
            m_items.append(SceneRect { tab, 0, ColorRole::BoxFill, ColorRole::GroupStroke, false });
            addText(keyword,
                    tab.adjusted(m_style.padding, m_style.padding / 2, -m_style.padding,
                                 -m_style.padding / 2),
                    Qt::AlignLeft);

            const qreal besideTab = frame.right - tab.right() - 2 * m_style.padding;
            const bool below = besideTab < minimumWrapWidth();
            const qreal conditionLeft
                = below ? frame.left + m_style.padding : tab.right() + m_style.padding;
            const qreal conditionTop
                = below ? tab.bottom() + m_style.padding / 2 : frame.top + m_style.padding / 2;
            const WrappedText condition = wrap(
                bracketed(group.label), below ? frameWidth - 2 * m_style.padding : besideTab);
            addLabel(condition,
                     QRectF(conditionLeft, conditionTop, condition.width, condition.height),
                     Qt::AlignLeft, ColorRole::MutedText);
            const qreal conditionBottom
                = condition.lines.isEmpty() ? frame.top : conditionTop + condition.height;
            m_y = std::max(tab.bottom(), conditionBottom);
        }

        void place(const GroupElse& otherwise)
        {
            if (m_groups.isEmpty())
                return;
            m_y += m_style.rowGap;
            const OpenGroup& frame = m_groups.last();
            m_items.append(SceneLine { { QPointF(frame.left, m_y), QPointF(frame.right, m_y) },
                                       ColorRole::GroupStroke,
                                       true,
                                       ArrowHead::None });
            const WrappedText condition
                = wrap(bracketed(otherwise.label), frame.right - frame.left - 2 * m_style.padding);
            addLabel(condition,
                     QRectF(frame.left + m_style.padding, m_y + m_style.padding / 2,
                            condition.width, condition.height),
                     Qt::AlignLeft, ColorRole::MutedText);
            m_y += condition.height + m_style.padding / 2;
        }

        void place(const GroupEnd&)
        {
            if (m_groups.isEmpty())
                return;
            m_y += m_style.padding;
            const OpenGroup frame = m_groups.takeLast();
            m_frames.append(SceneRect {
                QRectF(frame.left, frame.top, frame.right - frame.left, m_y - frame.top), 0,
                std::nullopt, ColorRole::GroupStroke, false });
        }

        const SequenceDiagram& m_diagram;
        const DiagramStyle& m_style;
        const TextMeasurer& m_measurer;
        qreal m_availableWidth = 0;
        QList<qreal> m_columnWidths;
        QList<qreal> m_columnLefts;
        qreal m_sceneWidth = 0;
        qreal m_y = 0;
        QList<OpenGroup> m_groups;
        QList<SceneItem> m_lifelines;
        QList<SceneItem> m_frames;
        QList<SceneItem> m_items;
    };
}

Scene SequenceLayout::layout(const SequenceDiagram& diagram, const DiagramStyle& style,
                             qreal availableWidth, const TextMeasurer& measurer)
{
    return SequenceLayouter(diagram, style, availableWidth, measurer).layout();
}
}
