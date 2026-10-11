#include "blocklayout.h"

#include "diagram/layoutsupport.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <map>

namespace diagram
{
namespace
{
    using layouting::WrappedText;

    constexpr qreal kUnbounded = std::numeric_limits<qreal>::infinity();
    constexpr qreal kFitTolerance = 0.001;
    constexpr qreal kObstacleClearance = 3;
    constexpr qreal kNodeDepth = 6;
    constexpr qreal kCylinderCap = 10;
    constexpr qreal kMarkerWidth = 14;
    constexpr qreal kMarkerHeight = 10;
    constexpr qreal kMarkerStub = 3;
    constexpr qreal kMinimumOverlap = 16;
    constexpr qreal kDetourSpacing = 6;

    using Route = QList<QPointF>;

    struct Obstacle
    {
        int block = -1;
        QRectF rect;
    };

    struct Fragment
    {
        qreal width = 0;
        qreal height = 0;
        QList<SceneItem> items;
        std::map<int, QRectF> rects;
        QList<Obstacle> obstacles;
    };

    struct Insets
    {
        qreal left = 0;
        qreal right = 0;
        qreal top = 0;
        qreal bottom = 0;

        qreal horizontal() const { return left + right; }
        qreal vertical() const { return top + bottom; }
    };

    struct NoteBox
    {
        WrappedText text;
        qreal width = 0;
        qreal height = 0;
    };

    qreal centred(qreal space) { return std::round(space / 2); }

    void merge(Fragment& target, const Fragment& part, qreal dx, qreal dy)
    {
        layouting::appendShifted(target.items, part.items, dx, dy);
        for (const auto& [block, rect] : part.rects)
            target.rects[block] = rect.translated(dx, dy);
        for (const Obstacle& obstacle : part.obstacles)
            target.obstacles.append({ obstacle.block, obstacle.rect.translated(dx, dy) });
    }

    bool crosses(const QPointF& from, const QPointF& to, const QRectF& obstacle)
    {
        const QRectF box = obstacle.adjusted(-kObstacleClearance, -kObstacleClearance,
                                             kObstacleClearance, kObstacleClearance);
        return std::min(from.x(), to.x()) <= box.right() && std::max(from.x(), to.x()) >= box.left()
            && std::min(from.y(), to.y()) <= box.bottom()
            && std::max(from.y(), to.y()) >= box.top();
    }

    bool isFree(const Route& route, const QList<Obstacle>& obstacles, int fromBlock, int toBlock)
    {
        for (qsizetype index = 1; index < route.size(); ++index)
        {
            for (const Obstacle& obstacle : obstacles)
            {
                if (obstacle.block == fromBlock || obstacle.block == toBlock)
                    continue;
                if (crosses(route.at(index - 1), route.at(index), obstacle.rect))
                    return false;
            }
        }
        return true;
    }

    class BlockLayouter
    {
    public:
        BlockLayouter(const BlockDiagram& diagram, const DiagramStyle& style, qreal availableWidth,
                      const TextMeasurer& measurer)
            : m_diagram(diagram)
            , m_style(style)
            , m_measurer(measurer)
            , m_wrapper(style, measurer)
            , m_availableWidth(availableWidth)
            , m_children(diagram.blocks.size())
        {
            for (qsizetype index = 0; index < diagram.blocks.size(); ++index)
            {
                const int parent = diagram.blocks.at(index).parent;
                (parent < 0 ? m_roots : m_children[parent]).append(int(index));
            }
            m_labelLimit = std::min(
                style.minLabelWidth,
                std::max<qreal>(0, availableWidth - 2 * style.margin - 2 * style.labelHalo));
            qreal labelHeight = 0;
            for (const BlockLink& link : diagram.links)
            {
                const WrappedText label = wrapAt(link.label, m_labelLimit);
                labelHeight = std::max(labelHeight, label.height);
                m_widestLabel = std::max(m_widestLabel, label.width);
            }
            m_rowGap = style.flowGap + labelHeight;
        }

        Scene layout()
        {
            const qreal contentWidth = std::max<qreal>(0, m_availableWidth - 2 * m_style.margin);
            const Fragment flow = layoutLevel(m_roots, contentWidth);
            const WrappedText title = m_wrapper.wrap(m_diagram.title, contentWidth);

            qreal top = m_style.margin;
            if (!title.lines.isEmpty())
                top += title.height + m_style.rowGap;
            const qreal totalWidth = std::max(flow.width, title.width);
            const qreal originX = m_style.margin + (totalWidth - flow.width) / 2;

            Fragment placed;
            merge(placed, flow, originX, top);
            const QList<Route> routes = routeLinks(placed, originX + flow.width + gap() / 2);
            qreal rightmost = originX + flow.width;
            for (const Route& route : routes)
            {
                for (const QPointF& point : route)
                    rightmost = std::max(rightmost, point.x());
            }
            const qreal sceneWidth = layouting::wholePixels(
                std::max({ totalWidth + 2 * m_style.margin, rightmost + m_style.margin,
                           m_widestLabel + 2 * m_style.labelHalo }));

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

            scene.items.append(placed.items);
            addLinks(scene, routes);
            return scene;
        }

    private:
        qreal pad() const { return m_style.padding; }

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

        int ancestorIn(int block, const QList<int>& siblings) const
        {
            for (int current = block; current >= 0; current = m_diagram.blocks.at(current).parent)
            {
                if (siblings.contains(current))
                    return current;
            }
            return -1;
        }

        const BlockNote* noteOn(int block, BlockNoteSide side) const
        {
            for (const BlockNote& note : m_diagram.notes)
            {
                if (note.block == block && note.side == side)
                    return &note;
            }
            return nullptr;
        }

        QList<QList<int>> layersOf(const QList<int>& siblings) const
        {
            const qsizetype count = siblings.size();
            QList<QList<int>> successors(count);
            for (const BlockLink& link : m_diagram.links)
            {
                const qsizetype from = siblings.indexOf(ancestorIn(link.from, siblings));
                const qsizetype to = siblings.indexOf(ancestorIn(link.to, siblings));
                if (from < 0 || to < 0 || from == to || successors[from].contains(int(to)))
                    continue;
                successors[from].append(int(to));
            }

            QList<int> state(count, 0);
            QList<QList<int>> kept(count);
            QList<int> finished;
            std::function<void(int)> visit = [&](int node)
            {
                state[node] = 1;
                for (const int next : std::as_const(successors[node]))
                {
                    if (state[next] == 1)
                        continue;
                    kept[node].append(next);
                    if (state[next] == 0)
                        visit(next);
                }
                state[node] = 2;
                finished.append(node);
            };
            for (int node = 0; node < count; ++node)
            {
                if (state[node] == 0)
                    visit(node);
            }

            QList<int> layerOf(count, 0);
            for (auto node = finished.crbegin(); node != finished.crend(); ++node)
            {
                for (const int next : std::as_const(kept[*node]))
                    layerOf[next] = std::max(layerOf[next], layerOf[*node] + 1);
            }
            QList<QList<int>> layers;
            for (int position = 0; position < count; ++position)
            {
                while (layers.size() <= layerOf[position])
                    layers.append(QList<int> {});
                layers[layerOf[position]].append(siblings.at(position));
            }
            return layers;
        }

        bool linksAnother(int block, const QList<int>& siblings) const
        {
            for (const BlockLink& link : m_diagram.links)
            {
                const int from = ancestorIn(link.from, siblings);
                const int to = ancestorIn(link.to, siblings);
                if ((from == block || to == block) && from >= 0 && to >= 0 && from != to)
                    return true;
            }
            return false;
        }

        QList<QList<int>> rowsOf(const QList<int>& layer, const QList<int>& siblings,
                                 qreal maxWidth) const
        {
            const bool squeezes = std::any_of(layer.cbegin(), layer.cend(), [&](int block)
                                              { return linksAnother(block, siblings); });
            QList<QList<int>> rows;
            qreal rowWidth = 0;
            for (const int block : layer)
            {
                const qreal width = squeezes ? minimal(block).width : natural(block).width;
                if (!rows.isEmpty() && rowWidth + gap() + width <= maxWidth + kFitTolerance)
                {
                    rows.last().append(block);
                    rowWidth += gap() + width;
                    continue;
                }
                rows.append(QList<int> { block });
                rowWidth = width;
            }
            return rows;
        }

        qreal gap() const { return m_style.flowGap; }

        const Fragment& natural(int block) const
        {
            auto found = m_natural.find(block);
            if (found == m_natural.end())
            {
                Fragment fragment = layoutItem(block, kUnbounded);
                found = m_natural.emplace(block, std::move(fragment)).first;
            }
            return found->second;
        }

        const Fragment& minimal(int block) const
        {
            auto found = m_minimal.find(block);
            if (found == m_minimal.end())
            {
                Fragment fragment = layoutItem(block, 0);
                found = m_minimal.emplace(block, std::move(fragment)).first;
            }
            return found->second;
        }

        QList<qreal> limitsOf(const QList<int>& row, qreal maxWidth) const
        {
            qreal naturalSum = 0;
            qreal minimalSum = 0;
            for (const int block : row)
            {
                naturalSum += natural(block).width;
                minimalSum += minimal(block).width;
            }
            const qreal budget = maxWidth - gap() * qreal(row.size() - 1);
            const qreal spare = std::max<qreal>(0, budget - minimalSum);
            const qreal room = naturalSum - minimalSum;
            QList<qreal> limits;
            for (const int block : row)
            {
                const qreal naturalWidth = natural(block).width;
                if (naturalSum <= budget + kFitTolerance || room <= 0)
                {
                    limits.append(naturalWidth);
                    continue;
                }
                const qreal minimalWidth = minimal(block).width;
                limits.append(minimalWidth + spare * (naturalWidth - minimalWidth) / room);
            }
            return limits;
        }

        Fragment layoutRow(const QList<int>& row, qreal maxWidth) const
        {
            const QList<qreal> limits = limitsOf(row, maxWidth);
            QList<Fragment> items;
            for (qsizetype index = 0; index < row.size(); ++index)
            {
                const int block = row.at(index);
                if (limits.at(index) >= natural(block).width - kFitTolerance)
                    items.append(natural(block));
                else if (limits.at(index) <= minimal(block).width + kFitTolerance)
                    items.append(minimal(block));
                else
                    items.append(layoutItem(block, limits.at(index)));
            }
            Fragment fragment;
            for (const Fragment& item : std::as_const(items))
                fragment.height = std::max(fragment.height, item.height);
            qreal x = 0;
            for (const Fragment& item : std::as_const(items))
            {
                merge(fragment, item, x, centred(fragment.height - item.height));
                x += item.width + gap();
            }
            fragment.width = x - gap();
            return fragment;
        }

        Fragment layoutLevel(const QList<int>& siblings, qreal maxWidth) const
        {
            QList<Fragment> rows;
            for (const QList<int>& layer : layersOf(siblings))
            {
                for (const QList<int>& row : rowsOf(layer, siblings, maxWidth))
                    rows.append(layoutRow(row, maxWidth));
            }
            Fragment level;
            for (const Fragment& row : std::as_const(rows))
                level.width = std::max(level.width, row.width);
            qreal y = 0;
            for (const Fragment& row : std::as_const(rows))
            {
                merge(level, row, centred(level.width - row.width), y);
                y += row.height + m_rowGap;
            }
            level.height = rows.isEmpty() ? 0 : y - m_rowGap;
            return level;
        }

        NoteBox noteBoxOf(const BlockNote& note, qreal limit) const
        {
            NoteBox box;
            box.text = wrapAt(note.text, limit - 2 * pad());
            box.width = box.text.width + 2 * pad();
            box.height = textHeight(box.text) + 2 * pad();
            return box;
        }

        void placeNote(Fragment& fragment, const NoteBox& box, qreal x, qreal y) const
        {
            const QRectF rect(x, y, box.width, box.height);
            fragment.items.append(
                SceneRect { rect, 0, ColorRole::NoteFill, ColorRole::NoteStroke, false });
            if (!box.text.lines.isEmpty())
                fragment.items.append(
                    SceneText { QRectF(x + pad(), y + pad(), box.text.width, box.text.height),
                                Qt::AlignLeft, box.text.lines, ColorRole::Text });
            fragment.obstacles.append({ -1, rect });
        }

        Fragment layoutItem(int block, qreal maxWidth) const
        {
            const BlockNote* left = noteOn(block, BlockNoteSide::Left);
            const BlockNote* right = noteOn(block, BlockNoteSide::Right);
            if (!left && !right)
                return layoutBlock(block, maxWidth);

            const int sides = (left ? 1 : 0) + (right ? 1 : 0);
            const qreal noteGap = m_style.columnGap;
            const qreal noteLimit
                = maxWidth == kUnbounded ? kUnbounded : (maxWidth - sides * noteGap) / (sides + 2);
            const NoteBox leftBox = left ? noteBoxOf(*left, noteLimit) : NoteBox {};
            const NoteBox rightBox = right ? noteBoxOf(*right, noteLimit) : NoteBox {};
            const qreal blockLimit = maxWidth == kUnbounded
                ? kUnbounded
                : maxWidth - leftBox.width - rightBox.width - sides * noteGap;
            const Fragment body = layoutBlock(block, blockLimit);

            Fragment fragment;
            fragment.height = std::max({ body.height, leftBox.height, rightBox.height });
            qreal x = 0;
            if (left)
            {
                placeNote(fragment, leftBox, x, centred(fragment.height - leftBox.height));
                x += leftBox.width + noteGap;
            }
            merge(fragment, body, x, centred(fragment.height - body.height));
            x += body.width;
            if (right)
            {
                x += noteGap;
                placeNote(fragment, rightBox, x, centred(fragment.height - rightBox.height));
                x += rightBox.width;
            }
            fragment.width = x;
            return fragment;
        }

        Fragment layoutBlock(int index, qreal maxWidth) const
        {
            if (m_diagram.blocks.at(index).isContainer)
                return layoutContainer(index, maxWidth);
            return layoutLeaf(index, maxWidth);
        }

        Insets insetsOf(BlockKind kind) const
        {
            const qreal p = pad();
            switch (kind)
            {
                case BlockKind::Node:
                    return { p, p + kNodeDepth, p + kNodeDepth, p };
                case BlockKind::Component:
                    return { p, 2 * p + kMarkerWidth, p, p };
                case BlockKind::Database:
                    return { p, p, kCylinderCap + p / 2, kCylinderCap / 2 + p / 2 };
                case BlockKind::Rectangle:
                    break;
            }
            return { p, p, p, p };
        }

        Fragment layoutLeaf(int index, qreal maxWidth) const
        {
            const Block& block = m_diagram.blocks.at(index);
            const Insets inset = insetsOf(block.kind);
            const WrappedText text = wrapAt(block.label, maxWidth - inset.horizontal());
            const qreal width = layouting::wholePixels(
                std::max(text.width + inset.horizontal(), m_style.minColumnWidth));
            const qreal height = layouting::wholePixels(textHeight(text) + inset.vertical());

            Fragment fragment;
            fragment.width = width;
            fragment.height = height;
            addShape(fragment.items, block.kind, width, height);
            if (!text.lines.isEmpty())
            {
                const QRectF content(inset.left, inset.top, width - inset.horizontal(),
                                     height - inset.vertical());
                fragment.items.append(SceneText { QRectF(content.center().x() - text.width / 2,
                                                         content.center().y() - text.height / 2,
                                                         text.width, text.height),
                                                  Qt::AlignHCenter, text.lines, ColorRole::Text });
            }
            const QRectF rect(0, 0, width, height);
            fragment.rects[index] = rect;
            fragment.obstacles.append({ index, rect });
            return fragment;
        }

        void addShape(QList<SceneItem>& items, BlockKind kind, qreal width, qreal height) const
        {
            switch (kind)
            {
                case BlockKind::Node:
                    addNode(items, width, height);
                    return;
                case BlockKind::Component:
                    addComponent(items, width, height);
                    return;
                case BlockKind::Database:
                    addDatabase(items, width, height);
                    return;
                case BlockKind::Rectangle:
                    break;
            }
            items.append(SceneRect { QRectF(0, 0, width, height), m_style.boxRadius,
                                     ColorRole::BoxFill, ColorRole::BoxStroke, false });
        }

        void addNode(QList<SceneItem>& items, qreal width, qreal height) const
        {
            const qreal depth = kNodeDepth;
            items.append(SceneRect { QRectF(0, depth, width - depth, height - depth), 0,
                                     ColorRole::BoxFill, ColorRole::BoxStroke, false });
            items.append(ScenePolygon { { QPointF(0, depth), QPointF(depth, 0), QPointF(width, 0),
                                          QPointF(width - depth, depth) },
                                        ColorRole::BoxFill,
                                        ColorRole::BoxStroke });
            items.append(
                ScenePolygon { { QPointF(width - depth, depth), QPointF(width, 0),
                                 QPointF(width, height - depth), QPointF(width - depth, height) },
                               ColorRole::BoxFill,
                               ColorRole::BoxStroke });
        }

        void addComponent(QList<SceneItem>& items, qreal width, qreal height) const
        {
            items.append(SceneRect { QRectF(0, 0, width, height), m_style.boxRadius,
                                     ColorRole::BoxFill, ColorRole::BoxStroke, false });
            const qreal markerLeft = width - pad() - kMarkerWidth;
            const qreal markerTop = pad();
            items.append(SceneRect { QRectF(markerLeft, markerTop, kMarkerWidth, kMarkerHeight), 0,
                                     ColorRole::BoxFill, ColorRole::BoxStroke, false });
            for (const qreal stubTop : { markerTop + 2, markerTop + 6 })
                items.append(
                    SceneRect { QRectF(markerLeft - kMarkerStub, stubTop, kMarkerStub, 2.5), 0,
                                ColorRole::BoxFill, ColorRole::BoxStroke, false });
        }

        void addDatabase(QList<SceneItem>& items, qreal width, qreal height) const
        {
            const qreal cap = kCylinderCap;
            items.append(SceneEllipse { QRectF(0, height - cap, width, cap), ColorRole::BoxFill,
                                        ColorRole::BoxStroke });
            items.append(SceneRect { QRectF(0, cap / 2, width, height - cap), 0, ColorRole::BoxFill,
                                     std::nullopt, false });
            for (const qreal x : { qreal(0), width })
                items.append(SceneLine { { QPointF(x, cap / 2), QPointF(x, height - cap / 2) },
                                         ColorRole::BoxStroke,
                                         false,
                                         ArrowHead::None });
            items.append(SceneEllipse { QRectF(0, 0, width, cap), ColorRole::BoxFill,
                                        ColorRole::BoxStroke });
        }

        Fragment layoutContainer(int index, qreal maxWidth) const
        {
            const Block& block = m_diagram.blocks.at(index);
            const qreal inset = pad() + m_style.groupInset;
            const WrappedText label = wrapAt(block.label, maxWidth - 2 * pad());
            const qreal headerHeight = label.lines.isEmpty() ? 0 : label.height + pad();
            const Fragment content = layoutLevel(m_children.at(index), maxWidth - 2 * inset);
            const qreal contentTop = headerHeight > 0 ? headerHeight + pad() : inset;

            Fragment fragment;
            fragment.width = layouting::wholePixels(std::max(
                { content.width + 2 * inset, label.width + 2 * pad(), m_style.minColumnWidth }));
            fragment.height = layouting::wholePixels(
                content.height > 0 ? contentTop + content.height + inset : headerHeight + inset);
            const QRectF frame(0, 0, fragment.width, fragment.height);
            fragment.items.append(SceneRect { frame, m_style.boxRadius, std::nullopt,
                                              ColorRole::GroupStroke, false });
            if (!label.lines.isEmpty())
                fragment.items.append(
                    SceneText { QRectF(pad(), pad() / 2, label.width, label.height), Qt::AlignLeft,
                                label.lines, ColorRole::Text });
            merge(fragment, content, centred(fragment.width - content.width), contentTop);
            fragment.rects[index] = frame;
            return fragment;
        }

        Route nestedRoute(const QRectF& from, const QRectF& to) const
        {
            const bool fromIsOuter = from.contains(to);
            const QRectF& outer = fromIsOuter ? from : to;
            const QRectF& inner = fromIsOuter ? to : from;
            const qreal y = inner.center().y();
            Route route { QPointF(outer.left(), y), QPointF(inner.left(), y) };
            if (!fromIsOuter)
                std::reverse(route.begin(), route.end());
            return route;
        }

        Route sideRoute(const QRectF& from, const QRectF& to) const
        {
            const qreal top = std::max(from.top(), to.top());
            const qreal bottom = std::min(from.bottom(), to.bottom());
            if (bottom > top)
            {
                const qreal y = (top + bottom) / 2;
                if (from.right() <= to.left())
                    return { QPointF(from.right(), y), QPointF(to.left(), y) };
                if (to.right() <= from.left())
                    return { QPointF(from.left(), y), QPointF(to.right(), y) };
            }
            return { from.center(), to.center() };
        }

        QList<Route> downwardCandidates(const QRectF& upper, const QRectF& lower,
                                        qreal detourX) const
        {
            const qreal startY = upper.bottom();
            const qreal endY = lower.top();
            const qreal fromX = upper.center().x();
            const qreal toX = lower.center().x();
            const qreal overlapLeft = std::max(upper.left(), lower.left());
            const qreal overlapRight = std::min(upper.right(), lower.right());

            QList<Route> candidates;
            if (overlapRight - overlapLeft >= kMinimumOverlap)
            {
                for (const qreal fraction : { 0.5, 0.25, 0.75 })
                {
                    const qreal x = overlapLeft + (overlapRight - overlapLeft) * fraction;
                    candidates.append({ QPointF(x, startY), QPointF(x, endY) });
                }
            }
            if (fromX != toX)
            {
                for (const qreal lane : { endY - m_rowGap / 2, startY + m_rowGap / 2 })
                    candidates.append({ QPointF(fromX, startY), QPointF(fromX, lane),
                                        QPointF(toX, lane), QPointF(toX, endY) });
            }
            if (candidates.isEmpty())
                candidates.append({ QPointF(fromX, startY), QPointF(toX, endY) });
            candidates.append({ QPointF(upper.right(), upper.center().y()),
                                QPointF(detourX, upper.center().y()),
                                QPointF(detourX, lower.center().y()),
                                QPointF(lower.right(), lower.center().y()) });
            return candidates;
        }

        Route route(const BlockLink& link, const Fragment& placed, qreal& detourX) const
        {
            const QRectF& from = placed.rects.at(link.from);
            const QRectF& to = placed.rects.at(link.to);
            if (from.contains(to) || to.contains(from))
                return nestedRoute(from, to);
            const bool fromIsUpper = from.center().y() <= to.center().y();
            const QRectF& upper = fromIsUpper ? from : to;
            const QRectF& lower = fromIsUpper ? to : from;
            if (upper.bottom() > lower.top())
                return sideRoute(from, to);

            const QList<Route> candidates = downwardCandidates(upper, lower, detourX);
            qsizetype chosenIndex = 0;
            for (qsizetype index = 0; index < candidates.size(); ++index)
            {
                if (isFree(candidates.at(index), placed.obstacles, link.from, link.to))
                {
                    chosenIndex = index;
                    break;
                }
            }
            if (chosenIndex == candidates.size() - 1)
                detourX += kDetourSpacing;
            Route chosen = candidates.at(chosenIndex);
            if (!fromIsUpper)
                std::reverse(chosen.begin(), chosen.end());
            return chosen;
        }

        QRectF labelBounds(const Route& route, const WrappedText& label) const
        {
            const QPointF start = route.first();
            const QPointF end = route.last();
            if (route.size() == 4)
            {
                const QPointF middle = (route.at(1) + route.at(2)) / 2;
                return QRectF(QPointF(middle.x() - label.width / 2, middle.y() - label.height / 2),
                              QSizeF(label.width, label.height));
            }
            if (start.y() == end.y())
                return QRectF(QPointF((start.x() + end.x()) / 2 - label.width / 2,
                                      start.y() - label.height - 2),
                              QSizeF(label.width, label.height));
            const qreal lowest = std::max(start.y(), end.y());
            const qreal centre
                = std::max(std::min(start.y(), end.y()) + label.height / 2, lowest - m_rowGap / 2);
            return QRectF(QPointF(start.x() + pad(), centre - label.height / 2),
                          QSizeF(label.width, label.height));
        }

        QRectF keptInside(QRectF bounds, const QSizeF& size) const
        {
            const qreal halo = m_style.labelHalo;
            const qreal left
                = std::max(halo, std::min(bounds.left(), size.width() - bounds.width() - halo));
            const qreal top
                = std::max(0.0, std::min(bounds.top(), size.height() - bounds.height()));
            bounds.moveTopLeft(QPointF(left, top));
            return bounds;
        }

        QList<Route> routeLinks(const Fragment& placed, qreal detourX) const
        {
            QList<Route> routes;
            for (const BlockLink& link : m_diagram.links)
                routes.append(route(link, placed, detourX));
            return routes;
        }

        void addLinks(Scene& scene, const QList<Route>& routes) const
        {
            for (qsizetype index = 0; index < routes.size(); ++index)
            {
                const BlockLink& link = m_diagram.links.at(index);
                Route path = routes.at(index);
                if (link.head == BlockLinkHead::ToSource)
                    std::reverse(path.begin(), path.end());
                scene.items.append(SceneLine {
                    path, ColorRole::Line, link.line == BlockLinkLine::Dotted,
                    link.head == BlockLinkHead::None ? ArrowHead::None : ArrowHead::Filled });
            }
            for (qsizetype index = 0; index < routes.size(); ++index)
            {
                const WrappedText label = wrapAt(m_diagram.links.at(index).label, m_labelLimit);
                if (label.lines.isEmpty())
                    continue;
                const QRectF bounds = keptInside(labelBounds(routes.at(index), label), scene.size);
                scene.items.append(
                    SceneRect { bounds.adjusted(-m_style.labelHalo, 0, m_style.labelHalo, 0), 0,
                                ColorRole::Background, std::nullopt, false });
                scene.items.append(
                    SceneText { bounds, Qt::AlignLeft, label.lines, ColorRole::MutedText });
            }
        }

        const BlockDiagram& m_diagram;
        const DiagramStyle& m_style;
        const TextMeasurer& m_measurer;
        layouting::TextWrapper m_wrapper;
        qreal m_availableWidth;
        QList<int> m_roots;
        QList<QList<int>> m_children;
        qreal m_rowGap = 0;
        qreal m_labelLimit = 0;
        qreal m_widestLabel = 0;
        mutable std::map<int, Fragment> m_natural;
        mutable std::map<int, Fragment> m_minimal;
    };
}

Scene BlockLayout::layout(const BlockDiagram& diagram, const DiagramStyle& style,
                          qreal availableWidth, const TextMeasurer& measurer)
{
    return BlockLayouter(diagram, style, availableWidth, measurer).layout();
}
}
