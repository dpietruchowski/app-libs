#include "activitylayout.h"

#include "diagram/layoutsupport.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <variant>

namespace diagram
{
namespace
{
    using layouting::WrappedText;

    constexpr qreal kUnbounded = std::numeric_limits<qreal>::infinity();

    constexpr qreal kFitTolerance = 0.001;

    struct Extent
    {
        qreal left = 0;
        qreal right = 0;

        qreal width() const { return left + right; }
        qreal narrower() const { return std::min(left, right); }
    };

    struct Need
    {
        Extent minimal;
        Extent natural;
    };

    struct Fragment
    {
        qreal left = 0;
        qreal right = 0;
        qreal height = 0;
        qreal entryX = 0;
        qreal entryDrop = 0;
        bool visible = false;
        bool continues = true;
        QList<SceneItem> items;
    };

    struct Diamond
    {
        WrappedText text;
        qreal width = 0;
        qreal height = 0;
    };

    struct LabelStack
    {
        QList<WrappedText> parts;
        qreal width = 0;
        qreal height = 0;

        bool isEmpty() const { return parts.isEmpty(); }
    };

    struct IfColumn
    {
        const ActivityIfBranch* branch = nullptr;
        bool isPad = false;
        Extent extent;
        qreal axis = 0;
        qreal bottom = 0;
        bool joins = false;
        Diamond diamond;
        Fragment body;
        LabelStack stack;
    };

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

    void appendShifted(QList<SceneItem>& target, const QList<SceneItem>& source, qreal dx, qreal dy)
    {
        for (const SceneItem& item : source)
            target.append(std::visit([dx, dy](const auto& typed) -> SceneItem
                                     { return shifted(typed, dx, dy); }, item));
    }

    void addLine(QList<SceneItem>& items, QList<QPointF> points, ArrowHead head = ArrowHead::None)
    {
        items.append(SceneLine { std::move(points), ColorRole::Line, false, head });
    }

    void descend(QList<QPointF>& points, qreal toX, qreal toY)
    {
        const QPointF from = points.last();
        if (from.x() != toX)
        {
            const qreal middle = (from.y() + toY) / 2;
            points.append(QPointF(from.x(), middle));
            points.append(QPointF(toX, middle));
        }
        points.append(QPointF(toX, toY));
    }

    bool isBranchEnd(const ActivityElement& element)
    {
        return std::holds_alternative<ActivityBranchEnd>(element.node);
    }

    QString firstLabel(const ActivityBlock& block)
    {
        if (block.empty() || isBranchEnd(block.front()))
            return QString();
        return block.front().incomingLabel;
    }

    QList<Extent> shareExtents(const QList<Need>& needs, qreal usable)
    {
        qreal naturalSum = 0;
        qreal minimalSum = 0;
        for (const Need& need : needs)
        {
            naturalSum += need.natural.width();
            minimalSum += need.minimal.width();
        }
        QList<Extent> extents;
        if (usable >= naturalSum || usable <= minimalSum)
        {
            const bool natural = usable >= naturalSum;
            for (const Need& need : needs)
                extents.append(natural ? need.natural : need.minimal);
            return extents;
        }
        const qreal portion = (usable - minimalSum) / (naturalSum - minimalSum);
        for (const Need& need : needs)
            extents.append(Extent {
                need.minimal.left + portion * (need.natural.left - need.minimal.left),
                need.minimal.right + portion * (need.natural.right - need.minimal.right) });
        return extents;
    }

    qreal totalWidth(const QList<Extent>& extents)
    {
        qreal total = 0;
        for (const Extent& extent : extents)
            total += extent.width();
        return total;
    }

    qreal axisOffset(qreal total, const Extent& room)
    {
        if (total > room.width() + kFitTolerance)
            return total / 2;
        return std::max(total - room.right, std::min(total / 2, room.left));
    }

    class ActivityLayouter
    {
    public:
        ActivityLayouter(const ActivityDiagram& diagram, const DiagramStyle& style,
                         qreal availableWidth, const TextMeasurer& measurer, bool insetPartitions)
            : m_diagram(diagram)
            , m_style(style)
            , m_measurer(measurer)
            , m_wrapper(style, measurer)
            , m_availableWidth(availableWidth)
            , m_insetPartitions(insetPartitions)
        {
        }

        Scene layout()
        {
            const qreal contentWidth = m_availableWidth - 2 * m_style.margin;
            const Fragment flow
                = layoutBlock(m_diagram.body, topLevelRoom(std::max<qreal>(0, contentWidth)));
            const WrappedText title
                = m_wrapper.wrap(m_diagram.title, std::max<qreal>(0, contentWidth));

            qreal top = m_style.margin;
            if (!title.lines.isEmpty())
                top += title.height + m_style.rowGap;
            const qreal flowWidth = flow.left + flow.right;
            const qreal totalWidth = std::max(flowWidth, title.width);
            const qreal sceneWidth = layouting::wholePixels(totalWidth + 2 * m_style.margin);
            const qreal axisX = m_style.margin + (totalWidth - flowWidth) / 2 + flow.left;

            Scene scene;
            scene.size
                = QSizeF(sceneWidth, layouting::wholePixels(top + flow.height + m_style.margin));
            scene.fontFamily = m_style.fontFamily;
            scene.fontPixelSize = m_style.fontPixelSize;
            scene.lineHeight = m_measurer.lineHeight();
            scene.ascent = m_measurer.ascent();
            scene.arrowHeadSize = m_style.arrowHeadSize;
            if (!title.lines.isEmpty())
                scene.items.append(SceneText { QRectF((sceneWidth - title.width) / 2,
                                                      m_style.margin, title.width, title.height),
                                               Qt::AlignHCenter, title.lines, ColorRole::Text });
            appendShifted(scene.items, flow.items, axisX, top);
            return scene;
        }

    private:
        qreal pad() const { return m_style.padding; }

        Extent topLevelRoom(qreal usable) const
        {
            const Need need = blockNeed(m_diagram.body);
            if (usable >= need.natural.width())
                return Extent { std::max(need.natural.left, usable / 2),
                                std::max(need.natural.right, usable / 2) };
            return shareExtents({ need }, usable).first();
        }

        qreal minimalWidth(const QString& text) const
        {
            return std::min(m_wrapper.widestWord(text), m_style.minLabelWidth);
        }

        WrappedText wrapAt(const QString& text, qreal maxWidth) const
        {
            return m_wrapper.wrap(text, std::max(maxWidth, minimalWidth(text)));
        }

        qreal textHeight(const WrappedText& text) const
        {
            return std::max(text.height, m_measurer.lineHeight());
        }

        LabelStack stackOf(const QStringList& labels, qreal maxWidth) const
        {
            LabelStack stack;
            for (const QString& label : labels)
            {
                const WrappedText wrapped = wrapAt(label, maxWidth);
                if (wrapped.lines.isEmpty())
                    continue;
                stack.parts.append(wrapped);
                stack.width = std::max(stack.width, wrapped.width);
                stack.height += wrapped.height;
            }
            return stack;
        }

        void placeStack(QList<SceneItem>& items, const LabelStack& stack, qreal x, qreal y) const
        {
            for (const WrappedText& part : stack.parts)
            {
                items.append(SceneText { QRectF(x, y, part.width, part.height), Qt::AlignLeft,
                                         part.lines, ColorRole::MutedText });
                y += part.height;
            }
        }

        qreal connectorGap(const LabelStack& stack) const
        {
            return stack.isEmpty() ? m_style.flowGap : std::max(m_style.flowGap, stack.height + 4);
        }

        Need blockNeed(const ActivityBlock& block) const
        {
            const auto cached = m_needs.find(&block);
            if (cached != m_needs.end())
                return cached->second;
            const Fragment minimal = layoutBlock(block, Extent {});
            const Fragment natural = layoutBlock(block, Extent { kUnbounded, kUnbounded });
            Need need;
            need.minimal = Extent { minimal.left, minimal.right };
            need.natural = Extent { std::max(need.minimal.left, natural.left),
                                    std::max(need.minimal.right, natural.right) };
            m_needs[&block] = need;
            return need;
        }

        Diamond diamond(const QString& condition, qreal maxWidth) const
        {
            const qreal inset = 2 * (m_style.diamondPoint + pad());
            Diamond shape;
            shape.text = wrapAt(condition, maxWidth - inset);
            shape.width = std::max(shape.text.width + inset, inset + 2 * pad());
            shape.height = textHeight(shape.text) + 2 * pad();
            return shape;
        }

        void drawDiamond(QList<SceneItem>& items, const Diamond& shape, qreal centre, qreal top,
                         qreal height) const
        {
            const qreal halfWidth = shape.width / 2;
            const qreal point = m_style.diamondPoint;
            const qreal middle = top + height / 2;
            items.append(ScenePolygon {
                { QPointF(centre - halfWidth, middle), QPointF(centre - halfWidth + point, top),
                  QPointF(centre + halfWidth - point, top), QPointF(centre + halfWidth, middle),
                  QPointF(centre + halfWidth - point, top + height),
                  QPointF(centre - halfWidth + point, top + height) },
                ColorRole::BoxFill,
                ColorRole::BoxStroke });
            if (!shape.text.lines.isEmpty())
                items.append(SceneText { QRectF(centre - shape.text.width / 2,
                                                middle - shape.text.height / 2, shape.text.width,
                                                shape.text.height),
                                         Qt::AlignLeft, shape.text.lines, ColorRole::Text });
        }

        static Extent nonNegative(const Extent& room)
        {
            return Extent { std::max<qreal>(room.left, 0), std::max<qreal>(room.right, 0) };
        }

        Fragment layoutBlock(const ActivityBlock& block, const Extent& room) const
        {
            Fragment result;
            qreal y = 0;
            bool placedAny = false;
            bool previousContinues = false;
            for (std::size_t index = 0; index < block.size(); ++index)
            {
                const ActivityElement& element = block[index];
                if (isBranchEnd(element))
                {
                    previousContinues = false;
                    continue;
                }
                const Fragment fragment = layoutElement(element, room);
                const bool connected = placedAny && previousContinues;
                const LabelStack labels = connected
                    ? stackOf({ element.incomingLabel }, room.right - pad())
                    : LabelStack();
                const qreal gap = !placedAny ? 0
                    : connected              ? connectorGap(labels)
                                             : m_style.flowGap;
                const qreal top = y + gap;
                if (connected)
                {
                    QList<QPointF> points { QPointF(0, y) };
                    descend(points, fragment.entryX, top + fragment.entryDrop);
                    addLine(result.items, points, ArrowHead::Filled);
                    placeStack(result.items, labels, pad(), y + 2);
                    result.right = std::max(result.right, pad() + labels.width);
                }
                appendShifted(result.items, fragment.items, 0, top);
                if (!placedAny && index == 0)
                {
                    result.entryX = fragment.entryX;
                    result.entryDrop = fragment.entryDrop;
                    result.visible = fragment.visible;
                }
                placedAny = true;
                y = top + fragment.height;
                result.left = std::max(result.left, fragment.left);
                result.right = std::max(result.right, fragment.right);
                previousContinues = fragment.continues;
            }
            result.height = y;
            result.continues = block.empty() || previousContinues;
            return result;
        }

        Fragment layoutElement(const ActivityElement& element, const Extent& room) const
        {
            return std::visit([this, &room](const auto& node)
                              { return place(node, nonNegative(room)); }, element.node);
        }

        Fragment place(const ActivityStart&, const Extent&) const
        {
            const qreal radius = m_style.nodeRadius;
            Fragment fragment;
            fragment.left = fragment.right = radius;
            fragment.height = 2 * radius;
            fragment.visible = true;
            fragment.items.append(SceneEllipse { QRectF(-radius, 0, 2 * radius, 2 * radius),
                                                 ColorRole::Line, std::nullopt });
            return fragment;
        }

        Fragment place(const ActivityStop& stop, const Extent&) const
        {
            const qreal radius = m_style.nodeRadius;
            Fragment fragment;
            fragment.left = fragment.right = radius;
            fragment.height = 2 * radius;
            fragment.visible = true;
            fragment.continues = false;
            fragment.items.append(SceneEllipse { QRectF(-radius, 0, 2 * radius, 2 * radius),
                                                 std::nullopt, ColorRole::Line });
            const qreal inner = radius * 0.6;
            if (stop.kind == FlowEnd::Stop)
            {
                fragment.items.append(
                    SceneEllipse { QRectF(-inner, radius - inner, 2 * inner, 2 * inner),
                                   ColorRole::Line, std::nullopt });
                return fragment;
            }
            addLine(fragment.items,
                    { QPointF(-inner, radius - inner), QPointF(inner, radius + inner) });
            addLine(fragment.items,
                    { QPointF(inner, radius - inner), QPointF(-inner, radius + inner) });
            return fragment;
        }

        Fragment place(const ActivityBranchEnd&, const Extent&) const
        {
            Fragment fragment;
            fragment.continues = false;
            return fragment;
        }

        qreal boxWidth(const WrappedText& text, qreal limit) const
        {
            const qreal wanted = text.width + 2 * pad();
            return std::min(std::max(wanted, m_style.minColumnWidth), std::max(limit, wanted));
        }

        Fragment place(const ActivityAction& action, const Extent& room) const
        {
            if (!action.note)
                return placeAction(action, room.narrower());
            return placeActionWithNote(action, room);
        }

        Fragment placeAction(const ActivityAction& action, qreal half) const
        {
            const WrappedText text = wrapAt(action.text, 2 * half - 2 * pad());
            const qreal width = boxWidth(text, 2 * half);
            const qreal height = textHeight(text) + 2 * pad();
            Fragment fragment;
            fragment.left = fragment.right = width / 2;
            fragment.height = height;
            fragment.visible = true;
            addActionBox(fragment.items, text, width, 0, height);
            return fragment;
        }

        void addActionBox(QList<SceneItem>& items, const WrappedText& text, qreal width, qreal top,
                          qreal height) const
        {
            items.append(SceneRect { QRectF(-width / 2, top, width, height), m_style.actionRadius,
                                     ColorRole::BoxFill, ColorRole::BoxStroke, false });
            if (!text.lines.isEmpty())
                items.append(SceneText { QRectF(-text.width / 2, top + (height - text.height) / 2,
                                                text.width, text.height),
                                         Qt::AlignLeft, text.lines, ColorRole::Text });
        }

        struct NoteBox
        {
            WrappedText text;
            qreal width = 0;
        };

        NoteBox noteBox(const ActivityNote& note, qreal limit) const
        {
            NoteBox box;
            box.text = wrapAt(note.text, limit - 2 * pad());
            box.width = boxWidth(box.text, limit);
            return box;
        }

        Fragment placeActionWithNote(const ActivityAction& action, const Extent& room) const
        {
            const ActivityNote& note = *action.note;
            const bool onLeft = note.side == ActivityNoteSide::Left;
            const qreal noteSide = onLeft ? room.left : room.right;
            const qreal otherSide = onLeft ? room.right : room.left;
            const qreal gap = m_style.columnGap;
            const qreal actionHalfLimit
                = std::max<qreal>(0, std::min(otherSide, noteSide - gap - noteBox(note, 0).width));
            const WrappedText text = wrapAt(action.text, 2 * actionHalfLimit - 2 * pad());
            const qreal width = boxWidth(text, 2 * actionHalfLimit);
            const NoteBox box = noteBox(note, noteSide - gap - width / 2);
            const WrappedText& noteText = box.text;
            const qreal noteWidth = box.width;

            const qreal actionHeight = textHeight(text) + 2 * pad();
            const qreal noteHeight = textHeight(noteText) + 2 * pad();
            const qreal height = std::max(actionHeight, noteHeight);
            const qreal actionTop = (height - actionHeight) / 2;
            const qreal noteTop = (height - noteHeight) / 2;
            const qreal noteLeft = onLeft ? -width / 2 - gap - noteWidth : width / 2 + gap;

            Fragment fragment;
            fragment.height = height;
            fragment.visible = true;
            fragment.entryDrop = actionTop;
            fragment.left = width / 2 + (onLeft ? gap + noteWidth : 0);
            fragment.right = width / 2 + (onLeft ? 0 : gap + noteWidth);
            addActionBox(fragment.items, text, width, actionTop, actionHeight);
            if (actionTop + actionHeight < height)
                addLine(fragment.items,
                        { QPointF(0, actionTop + actionHeight), QPointF(0, height) });
            fragment.items.append(SceneRect { QRectF(noteLeft, noteTop, noteWidth, noteHeight), 0,
                                              ColorRole::NoteFill, ColorRole::NoteStroke, false });
            if (!noteText.lines.isEmpty())
                fragment.items.append(SceneText {
                    QRectF(noteLeft + pad(), noteTop + pad(), noteText.width, noteText.height),
                    Qt::AlignLeft, noteText.lines, ColorRole::Text });
            return fragment;
        }

        Need columnNeed(const ActivityIfBranch& branch) const
        {
            const Need block = blockNeed(branch.block);
            const QStringList labels { branch.guard, firstLabel(branch.block) };
            Need need;
            for (const bool natural : { false, true })
            {
                const qreal limit = natural ? kUnbounded : 0;
                const LabelStack stack = stackOf(labels, limit);
                Extent extent = natural ? block.natural : block.minimal;
                if (!stack.isEmpty())
                    extent.right = std::max(extent.right, pad() + stack.width);
                if (!branch.isElse)
                {
                    const qreal reach
                        = diamond(branch.condition, limit).width / 2 + m_style.arrowHeadSize;
                    extent.left = std::max(extent.left, reach);
                    extent.right = std::max(extent.right, reach);
                }
                (natural ? need.natural : need.minimal) = extent;
            }
            need.natural.left = std::max(need.natural.left, need.minimal.left);
            need.natural.right = std::max(need.natural.right, need.minimal.right);
            return need;
        }

        Fragment place(const ActivityIf& node, const Extent& room) const
        {
            QList<IfColumn> columns;
            bool hasElse = false;
            for (const ActivityIfBranch& branch : node.branches)
            {
                IfColumn column;
                column.branch = &branch;
                columns.append(column);
                hasElse = hasElse || branch.isElse;
            }
            if (!hasElse)
            {
                IfColumn column;
                column.isPad = true;
                columns.append(column);
            }

            QList<Need> needs;
            for (const IfColumn& column : columns)
            {
                const Extent padExtent { m_style.arrowHeadSize, m_style.arrowHeadSize };
                needs.append(column.isPad ? Need { padExtent, padExtent }
                                          : columnNeed(*column.branch));
            }
            const QList<Extent> extents = shareExtents(needs, room.width());
            const qreal total = totalWidth(extents);
            const qreal reachLeft = axisOffset(total, room);
            qreal cursor = -reachLeft;
            qreal rowHeight = 0;
            qreal gapAfterRow = m_style.flowGap;
            for (qsizetype index = 0; index < columns.size(); ++index)
            {
                IfColumn& column = columns[index];
                column.extent = extents.at(index);
                column.axis = cursor + column.extent.left;
                cursor += column.extent.width();
                if (column.isPad)
                    continue;
                const ActivityIfBranch& branch = *column.branch;
                column.body = layoutBlock(branch.block, column.extent);
                column.stack = stackOf({ branch.guard, firstLabel(branch.block) },
                                       column.extent.right - pad());
                if (!column.stack.isEmpty())
                    gapAfterRow = std::max(gapAfterRow, column.stack.height + 4);
                if (!branch.isElse)
                {
                    column.diamond = diamond(
                        branch.condition, 2 * column.extent.narrower() - 2 * m_style.arrowHeadSize);
                    rowHeight = std::max(rowHeight, column.diamond.height);
                }
            }
            return composeIf(columns, rowHeight, gapAfterRow,
                             Extent { reachLeft, total - reachLeft });
        }

        Fragment composeIf(QList<IfColumn>& columns, qreal rowHeight, qreal gapAfterRow,
                           const Extent& span) const
        {
            const qreal middle = rowHeight / 2;
            const qreal blockTop = rowHeight + gapAfterRow;
            qreal deepest = rowHeight;
            bool anyJoins = false;
            for (IfColumn& column : columns)
            {
                const bool empty = column.isPad || column.branch->block.empty();
                column.joins = empty || column.body.continues;
                column.bottom = empty ? rowHeight : blockTop + column.body.height;
                deepest = std::max(deepest, column.bottom);
                anyJoins = anyJoins || column.joins;
            }
            const qreal mergeY = deepest + m_style.flowGap;

            Fragment fragment;
            fragment.left = span.left;
            fragment.right = span.right;
            fragment.entryX = columns.first().axis;
            fragment.visible = true;
            fragment.continues = anyJoins;
            fragment.height = anyJoins ? mergeY : deepest;

            const IfColumn* previousDiamond = nullptr;
            for (const IfColumn& column : columns)
            {
                const bool diamondColumn = !column.isPad && !column.branch->isElse;
                if (diamondColumn)
                {
                    drawDiamond(fragment.items, column.diamond, column.axis, 0, rowHeight);
                    if (previousDiamond)
                        addLine(
                            fragment.items,
                            { QPointF(previousDiamond->axis + previousDiamond->diamond.width / 2,
                                      middle),
                              QPointF(column.axis - column.diamond.width / 2, middle) },
                            ArrowHead::Filled);
                    placeStack(fragment.items, column.stack, column.axis + pad(), rowHeight + 2);
                    addBranchDrop(fragment, column,
                                  QList<QPointF> { QPointF(column.axis, rowHeight) }, blockTop,
                                  mergeY);
                    previousDiamond = &column;
                    continue;
                }
                const QPointF leaving(previousDiamond->axis + previousDiamond->diamond.width / 2,
                                      middle);
                QList<QPointF> points { leaving, QPointF(column.axis, middle) };
                if (column.isPad)
                {
                    descend(points, column.axis, mergeY);
                    addLine(fragment.items, points);
                    continue;
                }
                placeStack(fragment.items, column.stack, column.axis + pad(), rowHeight + 2);
                addBranchDrop(fragment, column, points, blockTop, mergeY);
            }

            if (anyJoins)
                addMerge(fragment, columns, mergeY);
            return fragment;
        }

        void addBranchDrop(Fragment& fragment, const IfColumn& column, QList<QPointF> points,
                           qreal blockTop, qreal mergeY) const
        {
            const ActivityBlock& block = column.branch->block;
            if (block.empty())
            {
                descend(points, column.axis, mergeY);
                addLine(fragment.items, points);
                return;
            }
            appendShifted(fragment.items, column.body.items, column.axis, blockTop);
            if (column.body.visible)
            {
                descend(points, column.axis + column.body.entryX, blockTop + column.body.entryDrop);
                addLine(fragment.items, points, ArrowHead::Filled);
            }
            else if (points.size() > 1)
            {
                addLine(fragment.items, points);
            }
            if (column.body.continues)
                addLine(fragment.items,
                        { QPointF(column.axis, column.bottom), QPointF(column.axis, mergeY) });
        }

        void addMerge(Fragment& fragment, const QList<IfColumn>& columns, qreal mergeY) const
        {
            qreal low = 0;
            qreal high = 0;
            for (const IfColumn& column : columns)
            {
                if (!column.joins)
                    continue;
                low = std::min(low, column.axis);
                high = std::max(high, column.axis);
            }
            if (high > low)
                addLine(fragment.items, { QPointF(low, mergeY), QPointF(high, mergeY) });
        }

        Fragment place(const ActivityWhile& node, const Extent& room) const
        {
            const WrappedText exitLabel = wrapAt(node.exitLabel, room.left / 2 - pad());
            const qreal leftGutter
                = m_style.loopGutter + (exitLabel.lines.isEmpty() ? 0 : pad() + exitLabel.width);
            const qreal rightGutter = m_style.loopGutter;
            const Extent bodyRoom
                = nonNegative(Extent { room.left - leftGutter, room.right - rightGutter });

            const Diamond shape = diamond(node.condition, 2 * bodyRoom.narrower());
            const Fragment body = layoutBlock(node.block, bodyRoom);
            const bool bodyEmpty = node.block.empty();
            const LabelStack stack
                = stackOf({ node.yesLabel, firstLabel(node.block) }, bodyRoom.right - pad());
            const qreal middle = shape.height / 2;
            const qreal bodyTop = shape.height + connectorGap(stack);
            const qreal bodyBottom = bodyEmpty ? bodyTop : bodyTop + body.height;
            const bool loopsBack = bodyEmpty || body.continues;
            const qreal loopY = bodyBottom + m_style.flowGap / 2;
            const qreal exitY = std::max((loopsBack ? loopY : bodyBottom) + m_style.flowGap / 2,
                                         middle + exitLabel.height + 6);
            const qreal rightReach = std::max({ shape.width / 2, body.right, pad() + stack.width });
            const qreal leftReach = std::max(shape.width / 2, body.left);
            const qreal rightX = rightReach + rightGutter;
            const qreal leftX = -(leftReach + leftGutter);

            Fragment fragment;
            fragment.left = -leftX;
            fragment.right = rightX;
            fragment.height = exitY;
            fragment.visible = true;
            drawDiamond(fragment.items, shape, 0, 0, shape.height);
            placeStack(fragment.items, stack, pad(), shape.height + 2);
            if (!exitLabel.lines.isEmpty())
                fragment.items.append(SceneText {
                    QRectF(leftX + pad(), middle + 2, exitLabel.width, exitLabel.height),
                    Qt::AlignLeft, exitLabel.lines, ColorRole::MutedText });

            if (bodyEmpty)
            {
                addLine(fragment.items, { QPointF(0, shape.height), QPointF(0, loopY) });
            }
            else
            {
                appendShifted(fragment.items, body.items, 0, bodyTop);
                if (body.visible)
                {
                    QList<QPointF> points { QPointF(0, shape.height) };
                    descend(points, body.entryX, bodyTop + body.entryDrop);
                    addLine(fragment.items, points, ArrowHead::Filled);
                }
                if (body.continues)
                    addLine(fragment.items, { QPointF(0, bodyBottom), QPointF(0, loopY) });
            }
            if (loopsBack)
                addLine(fragment.items,
                        { QPointF(0, loopY), QPointF(rightX, loopY), QPointF(rightX, middle),
                          QPointF(shape.width / 2, middle) },
                        ArrowHead::Filled);
            addLine(fragment.items,
                    { QPointF(-shape.width / 2, middle), QPointF(leftX, middle),
                      QPointF(leftX, exitY), QPointF(0, exitY) });
            return fragment;
        }

        Fragment place(const ActivityRepeat& node, const Extent& room) const
        {
            const WrappedText yesLabel = wrapAt(node.yesLabel, room.right / 2 - pad());
            const qreal rightGutter
                = m_style.loopGutter + (yesLabel.lines.isEmpty() ? 0 : pad() + yesLabel.width);
            const Extent bodyRoom = nonNegative(Extent { room.left, room.right - rightGutter });

            const Diamond shape = diamond(node.condition, 2 * bodyRoom.narrower());
            const Fragment body = layoutBlock(node.block, bodyRoom);
            const bool bodyEmpty = node.block.empty();
            const WrappedText exitLabel = wrapAt(node.exitLabel, bodyRoom.right - pad());
            const LabelStack firstStack
                = stackOf({ firstLabel(node.block) }, bodyRoom.right - pad());
            const qreal markSize = 2 * m_style.nodeRadius;
            const qreal bodyTop = markSize + connectorGap(firstStack);
            const qreal bodyBottom = bodyEmpty ? markSize : bodyTop + body.height;
            const qreal gapBefore
                = std::max(m_style.flowGap, yesLabel.height + 4 - shape.height / 2);
            const qreal conditionTop = bodyBottom + gapBefore;
            const qreal conditionMiddle = conditionTop + shape.height / 2;
            const qreal conditionBottom = conditionTop + shape.height;
            const qreal exitSpace = exitLabel.lines.isEmpty() ? 0 : exitLabel.height + 4;
            const qreal rightReach
                = std::max({ shape.width / 2, body.right, markSize / 2, pad() + exitLabel.width });
            const qreal rightX = rightReach + rightGutter;
            const bool reachesCondition = bodyEmpty || body.continues;

            Fragment fragment;
            fragment.left = std::max({ shape.width / 2, body.left, markSize / 2 });
            fragment.right = rightX;
            fragment.height = conditionBottom + exitSpace;
            fragment.visible = true;
            fragment.continues = reachesCondition;

            fragment.items.append(
                ScenePolygon { { QPointF(0, 0), QPointF(markSize / 2, markSize / 2),
                                 QPointF(0, markSize), QPointF(-markSize / 2, markSize / 2) },
                               ColorRole::BoxFill,
                               ColorRole::BoxStroke });
            placeStack(fragment.items, firstStack, pad(), markSize + 2);
            if (bodyEmpty)
            {
                addLine(fragment.items, { QPointF(0, markSize), QPointF(0, conditionTop) },
                        ArrowHead::Filled);
            }
            else
            {
                appendShifted(fragment.items, body.items, 0, bodyTop);
                if (body.visible)
                {
                    QList<QPointF> points { QPointF(0, markSize) };
                    descend(points, body.entryX, bodyTop + body.entryDrop);
                    addLine(fragment.items, points, ArrowHead::Filled);
                }
                if (body.continues)
                    addLine(fragment.items, { QPointF(0, bodyBottom), QPointF(0, conditionTop) },
                            ArrowHead::Filled);
            }
            drawDiamond(fragment.items, shape, 0, conditionTop, shape.height);
            if (!yesLabel.lines.isEmpty())
                fragment.items.append(SceneText {
                    QRectF(shape.width / 2 + pad(), conditionMiddle - 2 - yesLabel.height,
                           yesLabel.width, yesLabel.height),
                    Qt::AlignLeft, yesLabel.lines, ColorRole::MutedText });
            addLine(fragment.items,
                    { QPointF(shape.width / 2, conditionMiddle), QPointF(rightX, conditionMiddle),
                      QPointF(rightX, markSize / 2), QPointF(markSize / 2, markSize / 2) },
                    ArrowHead::Filled);
            if (exitSpace > 0)
            {
                fragment.items.append(SceneText {
                    QRectF(pad(), conditionBottom + 2, exitLabel.width, exitLabel.height),
                    Qt::AlignLeft, exitLabel.lines, ColorRole::MutedText });
                addLine(fragment.items,
                        { QPointF(0, conditionBottom), QPointF(0, fragment.height) });
            }
            return fragment;
        }

        Fragment place(const ActivityFork& node, const Extent& room) const
        {
            QList<Need> needs;
            for (const ActivityBlock& block : node.branches)
            {
                Need need = blockNeed(block);
                for (const bool unbounded : { false, true })
                {
                    const LabelStack stack
                        = stackOf({ firstLabel(block) }, unbounded ? kUnbounded : 0);
                    if (stack.isEmpty())
                        continue;
                    Extent& extent = unbounded ? need.natural : need.minimal;
                    extent.right = std::max(extent.right, pad() + stack.width);
                }
                need.natural.right = std::max(need.natural.right, need.minimal.right);
                needs.append(need);
            }
            const Extent barRoom { room.left - pad(), room.right - pad() };
            const QList<Extent> extents = shareExtents(needs, barRoom.width());
            const qreal total = totalWidth(extents);

            struct Branch
            {
                qreal axis = 0;
                Fragment body;
                LabelStack stack;
            };
            QList<Branch> branches;
            qreal cursor = -axisOffset(total, barRoom);
            qreal gap = m_style.flowGap;
            for (qsizetype index = 0; index < extents.size(); ++index)
            {
                Branch branch;
                branch.axis = cursor + extents.at(index).left;
                cursor += extents.at(index).width();
                const ActivityBlock& block = node.branches[std::size_t(index)];
                branch.body = layoutBlock(block, extents.at(index));
                branch.stack = stackOf({ firstLabel(block) }, extents.at(index).right - pad());
                gap = std::max(gap, connectorGap(branch.stack));
                branches.append(branch);
            }

            const qreal barHeight = m_style.forkBarHeight;
            const qreal blockTop = barHeight + gap;
            qreal deepest = barHeight;
            bool anyJoins = false;
            for (qsizetype index = 0; index < branches.size(); ++index)
            {
                const ActivityBlock& block = node.branches[std::size_t(index)];
                const Branch& branch = branches.at(index);
                if (!block.empty())
                    deepest = std::max(deepest, blockTop + branch.body.height);
                anyJoins = anyJoins || block.empty() || branch.body.continues;
            }
            const qreal bottomBarY = deepest + m_style.flowGap;

            qreal contentLeft = branches.first().axis - branches.first().body.left;
            qreal contentRight = branches.last().axis + branches.last().body.right;
            for (qsizetype index = 0; index < branches.size(); ++index)
            {
                const Branch& branch = branches.at(index);
                const bool drawn = !node.branches[std::size_t(index)].empty();
                contentLeft = std::min(contentLeft, branch.axis - (drawn ? branch.body.left : 0.0));
                contentRight
                    = std::max(contentRight, branch.axis + (drawn ? branch.body.right : 0.0));
            }
            const qreal barLeft = contentLeft - pad();
            const qreal barRight = contentRight + pad();

            Fragment fragment;
            fragment.left = -barLeft;
            fragment.right = barRight;
            fragment.visible = true;
            fragment.continues = anyJoins;
            fragment.height = anyJoins ? bottomBarY + barHeight : deepest;
            fragment.items.append(SceneRect { QRectF(barLeft, 0, barRight - barLeft, barHeight), 0,
                                              ColorRole::Line, std::nullopt, false });
            for (qsizetype index = 0; index < branches.size(); ++index)
            {
                const ActivityBlock& block = node.branches[std::size_t(index)];
                const Branch& branch = branches.at(index);
                placeStack(fragment.items, branch.stack, branch.axis + pad(), barHeight + 2);
                if (block.empty())
                {
                    addLine(fragment.items,
                            { QPointF(branch.axis, barHeight), QPointF(branch.axis, bottomBarY) },
                            ArrowHead::Filled);
                    continue;
                }
                appendShifted(fragment.items, branch.body.items, branch.axis, blockTop);
                if (branch.body.visible)
                {
                    QList<QPointF> points { QPointF(branch.axis, barHeight) };
                    descend(points, branch.axis + branch.body.entryX,
                            blockTop + branch.body.entryDrop);
                    addLine(fragment.items, points, ArrowHead::Filled);
                }
                if (branch.body.continues)
                    addLine(fragment.items,
                            { QPointF(branch.axis, blockTop + branch.body.height),
                              QPointF(branch.axis, bottomBarY) },
                            ArrowHead::Filled);
            }
            if (anyJoins)
                fragment.items.append(
                    SceneRect { QRectF(barLeft, bottomBarY, barRight - barLeft, barHeight), 0,
                                ColorRole::Line, std::nullopt, false });
            return fragment;
        }

        qsizetype partitionLevels() const
        {
            if (!m_insetPartitions)
                return 0;
            const qreal contentWidth = m_availableWidth - 2 * m_style.margin;
            const qreal spare = contentWidth - m_style.minLabelWidth / 2;
            const auto fitting = qsizetype(std::max(0.0, spare / (2 * m_style.groupInset)));
            return std::min(qsizetype(m_style.maxGroupIndent), fitting);
        }

        Fragment place(const ActivityPartition& node, const Extent& room) const
        {
            const qreal inset = m_depth < partitionLevels() ? m_style.groupInset : 0;
            const WrappedText label = wrapAt(node.label, room.left - 3 * pad());
            const qreal tabWidth = label.width + 2 * pad();
            const qreal tabHeight = label.height + pad();
            const Extent bodyRoom = nonNegative(Extent { room.left - inset, room.right - inset });

            ++m_depth;
            const Fragment body = layoutBlock(node.block, bodyRoom);
            --m_depth;

            const bool bodyEmpty = node.block.empty();
            const LabelStack stack = stackOf({ firstLabel(node.block) }, bodyRoom.right - pad());
            const qreal innerGap = std::max(m_style.flowGap / 2, stack.height + 4);
            const qreal bodyTop = tabHeight + innerGap;
            const qreal bodyBottom = bodyEmpty ? tabHeight : bodyTop + body.height;
            const qreal bottom = bodyBottom + m_style.flowGap / 2;
            const qreal left = std::max(body.left + inset, tabWidth + pad());
            const qreal right = std::max(body.right + inset, pad() + stack.width);

            Fragment fragment;
            fragment.left = left;
            fragment.right = right;
            fragment.height = bottom;
            fragment.visible = true;
            fragment.continues = body.continues;
            if (body.visible)
            {
                fragment.entryX = body.entryX;
                fragment.entryDrop = bodyTop + body.entryDrop;
            }
            fragment.items.append(SceneRect { QRectF(-left, 0, left + right, bottom), 0,
                                              std::nullopt, ColorRole::GroupStroke, false });
            layouting::placeFrameTab(fragment.items, label, m_style, QPointF(-left, 0));
            placeStack(fragment.items, stack, pad(), tabHeight + 2);
            appendShifted(fragment.items, body.items, 0, bodyTop);
            if (body.continues)
                addLine(fragment.items,
                        { QPointF(0, bodyEmpty ? 0 : bodyBottom), QPointF(0, bottom) });
            return fragment;
        }

        const ActivityDiagram& m_diagram;
        const DiagramStyle& m_style;
        const TextMeasurer& m_measurer;
        layouting::TextWrapper m_wrapper;
        qreal m_availableWidth = 0;
        bool m_insetPartitions = true;
        mutable qsizetype m_depth = 0;
        mutable std::map<const ActivityBlock*, Need> m_needs;
    };
}

Scene ActivityLayout::layout(const ActivityDiagram& diagram, const DiagramStyle& style,
                             qreal availableWidth, const TextMeasurer& measurer)
{
    Scene scene = ActivityLayouter(diagram, style, availableWidth, measurer, true).layout();
    if (scene.size.width() <= availableWidth)
        return scene;
    Scene flat = ActivityLayouter(diagram, style, availableWidth, measurer, false).layout();
    return flat.size.width() < scene.size.width() ? flat : scene;
}
}
