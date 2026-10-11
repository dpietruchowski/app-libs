#include "blocklayout.h"

#include "diagram/layoutsupport.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace diagram
{
namespace
{
    using layouting::centred;
    using layouting::Fragment;
    using layouting::merge;
    using layouting::NoteBox;
    using layouting::Route;
    using layouting::WrappedText;

    constexpr qreal kUnbounded = std::numeric_limits<qreal>::infinity();
    constexpr qreal kNodeDepth = 6;
    constexpr qreal kCylinderCap = 10;
    constexpr qreal kMarkerWidth = 14;
    constexpr qreal kMarkerHeight = 10;
    constexpr qreal kMarkerStub = 3;
    constexpr qreal kMinimumOverlap = 16;
    constexpr qreal kDetourSpacing = 6;

    struct Insets
    {
        qreal left = 0;
        qreal right = 0;
        qreal top = 0;
        qreal bottom = 0;

        qreal horizontal() const { return left + right; }
        qreal vertical() const { return top + bottom; }
    };

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
            , m_flow(layouting::LayeredFlowSource { [this](int block, qreal maxWidth)
                                                    { return layoutItem(block, maxWidth); },
                                                    [this](const QList<int>& siblings)
                                                    { return layersOf(siblings); },
                                                    [this](int block, const QList<int>& siblings)
                                                    { return linksAnother(block, siblings); } },
                     style.flowGap)
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
            m_flow.setRowGap(m_rowGap);
        }

        Scene layout()
        {
            const qreal contentWidth = std::max<qreal>(0, m_availableWidth - 2 * m_style.margin);
            const Fragment flow = m_flow.layoutLevel(m_roots, contentWidth);
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

        WrappedText wrapAt(const QString& text, qreal maxWidth) const
        {
            return m_wrapper.wrapKeepingWords(text, maxWidth);
        }

        qreal textHeight(const WrappedText& text) const { return m_wrapper.heightOf(text); }

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
            QList<int> visitOrder;
            for (int node = 0; node < count; ++node)
                visitOrder.append(node);
            return layouting::groupedByLayer(siblings,
                                             layouting::longestPathLayers(successors, visitOrder));
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

        qreal gap() const { return m_style.flowGap; }

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
            const NoteBox leftBox = left
                ? layouting::noteBoxOf(left->text, noteLimit, m_wrapper, m_style)
                : NoteBox {};
            const NoteBox rightBox = right
                ? layouting::noteBoxOf(right->text, noteLimit, m_wrapper, m_style)
                : NoteBox {};
            const qreal blockLimit = maxWidth == kUnbounded
                ? kUnbounded
                : maxWidth - leftBox.width - rightBox.width - sides * noteGap;
            const Fragment body = layoutBlock(block, blockLimit);

            Fragment fragment;
            fragment.height = std::max({ body.height, leftBox.height, rightBox.height });
            qreal x = 0;
            if (left)
            {
                layouting::placeNote(fragment, leftBox, x,
                                     centred(fragment.height - leftBox.height), m_style);
                x += leftBox.width + noteGap;
            }
            merge(fragment, body, x, centred(fragment.height - body.height));
            x += body.width;
            if (right)
            {
                x += noteGap;
                layouting::placeNote(fragment, rightBox, x,
                                     centred(fragment.height - rightBox.height), m_style);
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
            const Fragment content = m_flow.layoutLevel(m_children.at(index), maxWidth - 2 * inset);
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
                if (layouting::isFree(candidates.at(index), placed.obstacles, link.from, link.to))
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
                const QRectF bounds = layouting::keptInside(labelBounds(routes.at(index), label),
                                                            scene.size, m_style.labelHalo);
                layouting::appendHaloLabel(scene.items, bounds, label, m_style.labelHalo);
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
        layouting::LayeredFlow m_flow;
    };
}

Scene BlockLayout::layout(const BlockDiagram& diagram, const DiagramStyle& style,
                          qreal availableWidth, const TextMeasurer& measurer)
{
    return BlockLayouter(diagram, style, availableWidth, measurer).layout();
}
}
