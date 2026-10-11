#include "statelayout.h"

#include "diagram/layoutsupport.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <utility>

namespace diagram
{
namespace
{
    using layouting::alignedX;
    using layouting::centred;
    using layouting::downRouteVia;
    using layouting::Fragment;
    using layouting::kSameCoordinate;
    using layouting::merge;
    using layouting::NoteBox;
    using layouting::Route;
    using layouting::Side;
    using layouting::withoutRepeats;
    using layouting::WrappedText;

    constexpr qreal kUnbounded = std::numeric_limits<qreal>::infinity();
    constexpr qreal kLoopHeight = 14;
    constexpr qreal kLoopBandSpacing = 2;
    constexpr qreal kLoopLabelGap = 4;
    constexpr int kMostStackedLabels = 2;

    enum class EdgeKind
    {
        Nested,
        Beside,
        Down,
        Around
    };

    struct Edge
    {
        int transition = -1;
        int from = -1;
        int to = -1;
        QRectF fromRect;
        QRectF toRect;
        EdgeKind kind = EdgeKind::Down;
        Side exit = Side::Bottom;
        Side entry = Side::Top;
        QPointF start;
        QPointF end;
        Route route;
    };

    qreal verticalOverlap(const QRectF& a, const QRectF& b)
    {
        return std::min(a.bottom(), b.bottom()) - std::max(a.top(), b.top());
    }

    qreal rightmostEdge(const Fragment& placed)
    {
        qreal right = 0;
        for (const auto& entry : placed.rects)
            right = std::max(right, entry.second.right());
        for (const layouting::Obstacle& obstacle : placed.obstacles)
            right = std::max(right, obstacle.rect.right());
        return right;
    }

    class EdgeRouter
    {
    public:
        EdgeRouter(const StateDiagram& diagram, const Fragment& placed, qreal rowGap,
                   const QList<int>& transitions)
            : m_diagram(diagram)
            , m_placed(placed)
            , m_rowGap(rowGap)
            , m_contentRight(rightmostEdge(placed))
        {
            for (const int index : transitions)
            {
                const StateTransition& transition = m_diagram.transitions.at(index);
                if (transition.from == transition.to)
                    continue;
                Edge edge;
                edge.transition = index;
                edge.from = transition.from;
                edge.to = transition.to;
                edge.fromRect = m_placed.rects.at(edge.from);
                edge.toRect = m_placed.rects.at(edge.to);
                decide(edge);
                m_edges.append(edge);
            }
            allocatePorts(m_edges);
            while (sendBlockedDownEdgesAround(m_edges))
                allocatePorts(m_edges);
            routeNestedAndBeside(m_edges);
            routeDown(m_edges);
        }

        QList<Route> directRoutes() const
        {
            QList<Route> routes(m_diagram.transitions.size());
            for (const Edge& edge : m_edges)
            {
                if (edge.kind != EdgeKind::Around)
                    routes[edge.transition] = edge.route;
            }
            return routes;
        }

        bool goesAround(qsizetype transition) const
        {
            return std::any_of(
                m_edges.cbegin(), m_edges.cend(), [transition](const Edge& edge)
                { return edge.kind == EdgeKind::Around && edge.transition == transition; });
        }

        QList<Route> aroundRoutes(const QList<QRectF>& labels,
                                  const QList<qreal>& labelWidths) const
        {
            QList<Edge> edges = m_edges;
            routeAround(edges, labels, labelWidths);
            QList<Route> routes(m_diagram.transitions.size());
            for (const Edge& edge : std::as_const(edges))
            {
                if (edge.kind == EdgeKind::Around)
                    routes[edge.transition] = edge.route;
            }
            return routes;
        }

    private:
        QRectF headerRectOf(int state) const
        {
            const QRectF rect = m_placed.rects.at(state);
            if (!m_diagram.states.at(state).isComposite)
                return rect;
            for (const layouting::Obstacle& obstacle : m_placed.obstacles)
            {
                if (obstacle.block != state)
                    continue;
                const qreal margin = obstacle.rect.top() - rect.top();
                return QRectF(rect.left(), rect.top(), rect.width(),
                              obstacle.rect.bottom() + margin - rect.top());
            }
            return rect;
        }

        bool bandIsFree(int state, const Edge& edge) const
        {
            const QRectF rect = m_placed.rects.at(state);
            const qreal y = headerRectOf(state).center().y();
            const Route band { QPointF(rect.right(), y), QPointF(m_contentRight + 1, y) };
            return layouting::isFree(band, m_placed.obstacles, edge.from, edge.to);
        }

        bool sendBlockedDownEdgesAround(QList<Edge>& edges) const
        {
            bool changed = false;
            for (Edge& edge : edges)
            {
                if (edge.kind != EdgeKind::Down)
                    continue;
                const Route route = downRouteVia(edge.start, edge.end);
                if (layouting::isFree(route, m_placed.obstacles, edge.from, edge.to))
                    continue;
                edge.kind = EdgeKind::Around;
                edge.exit = Side::Right;
                edge.entry = Side::Right;
                changed = true;
            }
            return changed;
        }

        bool downIsFree(const Edge& edge) const
        {
            const auto [exitX, entryX]
                = alignedX(edge.fromRect, edge.toRect, edge.fromRect.center().x(),
                           edge.toRect.center().x(), true, true);
            const Route tentative = downRouteVia(QPointF(exitX, edge.fromRect.bottom()),
                                                 QPointF(entryX, edge.toRect.top()));
            return layouting::isFree(tentative, m_placed.obstacles, edge.from, edge.to);
        }

        void decide(Edge& edge) const
        {
            const QRectF& from = edge.fromRect;
            const QRectF& to = edge.toRect;
            if (from.contains(to) || to.contains(from))
            {
                edge.kind = EdgeKind::Nested;
                return;
            }
            const bool apart = from.right() <= to.left() || to.right() <= from.left();
            if (verticalOverlap(from, to) > 0 && apart)
            {
                edge.kind = EdgeKind::Beside;
                return;
            }
            if (from.bottom() <= to.top())
            {
                if (downIsFree(edge))
                {
                    edge.kind = EdgeKind::Down;
                    return;
                }
                edge.kind = EdgeKind::Around;
                edge.exit = Side::Right;
                edge.entry = Side::Right;
                return;
            }
            edge.kind = EdgeKind::Around;
            edge.exit = bandIsFree(edge.from, edge) ? Side::Right : Side::Top;
            edge.entry = bandIsFree(edge.to, edge) ? Side::Right : Side::Top;
        }

        void allocatePorts(QList<Edge>& edges) const
        {
            QList<layouting::PortEnd> ends;
            for (qsizetype index = 0; index < edges.size(); ++index)
            {
                const Edge& edge = edges.at(index);
                if (edge.kind != EdgeKind::Down && edge.kind != EdgeKind::Around)
                    continue;
                ends.append({ int(index), true, edge.from, edge.exit, edge.toRect });
                ends.append({ int(index), false, edge.to, edge.entry, edge.fromRect });
            }
            const layouting::PortPlacements placements = layouting::allocatedPorts(
                ends,
                [this](int state, Side side)
                {
                    return side == Side::Left || side == Side::Right ? headerRectOf(state)
                                                                     : m_placed.rects.at(state);
                },
                [this](int state) { return m_placed.rects.at(state).center().y(); });
            for (qsizetype index = 0; index < edges.size(); ++index)
            {
                Edge& edge = edges[index];
                if (edge.kind != EdgeKind::Down && edge.kind != EdgeKind::Around)
                    continue;
                const layouting::PortPlacement& exit = placements.at({ int(index), true });
                const layouting::PortPlacement& entry = placements.at({ int(index), false });
                edge.start = exit.point;
                edge.end = entry.point;
                if (edge.kind != EdgeKind::Down)
                    continue;
                const auto [exitX, entryX] = alignedX(edge.fromRect, edge.toRect, edge.start.x(),
                                                      edge.end.x(), exit.alone, entry.alone);
                edge.start.setX(exitX);
                edge.end.setX(entryX);
            }
        }

        static Route nestedRoute(const QRectF& from, const QRectF& to)
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

        void routeNestedAndBeside(QList<Edge>& edges) const
        {
            std::map<std::pair<int, int>, int> pairCount;
            for (const Edge& edge : std::as_const(edges))
            {
                if (edge.kind == EdgeKind::Beside)
                    ++pairCount[{ std::min(edge.from, edge.to), std::max(edge.from, edge.to) }];
            }
            std::map<std::pair<int, int>, int> pairSeen;
            for (Edge& edge : edges)
            {
                if (edge.kind == EdgeKind::Nested)
                {
                    edge.route = nestedRoute(edge.fromRect, edge.toRect);
                    continue;
                }
                if (edge.kind != EdgeKind::Beside)
                    continue;
                const std::pair<int, int> pair { std::min(edge.from, edge.to),
                                                 std::max(edge.from, edge.to) };
                const int position = pairSeen[pair]++;
                const qreal low = std::max(edge.fromRect.top(), edge.toRect.top());
                const qreal high = std::min(edge.fromRect.bottom(), edge.toRect.bottom());
                const qreal fraction = qreal(position + 1) / qreal(pairCount.at(pair) + 1);
                const qreal y = std::round(low + (high - low) * fraction);
                if (edge.fromRect.right() <= edge.toRect.left())
                    edge.route
                        = { QPointF(edge.fromRect.right(), y), QPointF(edge.toRect.left(), y) };
                else
                    edge.route
                        = { QPointF(edge.fromRect.left(), y), QPointF(edge.toRect.right(), y) };
            }
        }

        static void routeDown(QList<Edge>& edges)
        {
            QList<layouting::DownEnds> ends;
            QList<qsizetype> indices;
            for (qsizetype index = 0; index < edges.size(); ++index)
            {
                const Edge& edge = edges.at(index);
                if (edge.kind != EdgeKind::Down)
                    continue;
                ends.append({ edge.from, edge.start, edge.end });
                indices.append(index);
            }
            const QList<Route> routes = layouting::routedDown(ends);
            for (qsizetype position = 0; position < indices.size(); ++position)
                edges[indices.at(position)].route = routes.at(position);
        }

        qreal gapAbove(const QRectF& rect) const
        {
            qreal top = rect.top();
            for (const layouting::Obstacle& obstacle : m_placed.obstacles)
            {
                const bool sharesRow
                    = obstacle.rect.bottom() > rect.top() && obstacle.rect.top() < rect.bottom();
                if (sharesRow)
                    top = std::min(top, obstacle.rect.top());
            }
            return std::max<qreal>(1, top - std::round(m_rowGap / 2));
        }

        qreal exitLevel(const Edge& edge) const
        {
            return edge.exit == Side::Top ? gapAbove(edge.fromRect) : edge.start.y();
        }

        qreal entryLevel(const Edge& edge) const
        {
            return edge.entry == Side::Top ? gapAbove(edge.toRect) : edge.end.y();
        }

        Route aroundRoute(const Edge& edge, qreal laneX) const
        {
            Route route { edge.start };
            if (edge.exit == Side::Top)
                route.append(QPointF(edge.start.x(), exitLevel(edge)));
            route.append(QPointF(laneX, exitLevel(edge)));
            route.append(QPointF(laneX, entryLevel(edge)));
            if (edge.entry == Side::Top)
                route.append(QPointF(edge.end.x(), entryLevel(edge)));
            route.append(edge.end);
            return withoutRepeats(route);
        }

        qreal rightEdgeWithin(qreal low, qreal high, const QList<QRectF>& labels) const
        {
            qreal right = 0;
            const auto consider = [&](const QRectF& rect)
            {
                if (rect.bottom() > low && rect.top() < high)
                    right = std::max(right, rect.right());
            };
            for (const auto& entry : m_placed.rects)
                consider(entry.second);
            for (const layouting::Obstacle& obstacle : m_placed.obstacles)
                consider(obstacle.rect);
            for (const QRectF& label : labels)
                consider(label);
            return right;
        }

        void routeAround(QList<Edge>& edges, const QList<QRectF>& labels,
                         const QList<qreal>& labelWidths) const
        {
            QList<qsizetype> around;
            QList<layouting::LaneRequest> requests;
            for (qsizetype index = 0; index < edges.size(); ++index)
            {
                const Edge& edge = edges.at(index);
                if (edge.kind != EdgeKind::Around)
                    continue;
                around.append(index);
                layouting::LaneRequest request;
                request.low = std::min(exitLevel(edge), entryLevel(edge));
                request.high = std::max(exitLevel(edge), entryLevel(edge));
                request.labelWidth = labelWidths.at(edge.transition);
                request.route = [this, &edge](qreal laneX) { return aroundRoute(edge, laneX); };
                requests.append(request);
            }
            const QList<Route> routes = layouting::routedInLanes(
                requests, [this, &labels](qreal low, qreal high)
                { return rightEdgeWithin(low, high, labels); });
            for (qsizetype position = 0; position < around.size(); ++position)
                edges[around.at(position)].route = routes.at(position);
        }

        const StateDiagram& m_diagram;
        const Fragment& m_placed;
        qreal m_rowGap;
        qreal m_contentRight;
        QList<Edge> m_edges;
    };

    struct LoopLabel
    {
        WrappedText text;
        qreal band = 0;
    };

    class StateLayouter
    {
    public:
        StateLayouter(const StateDiagram& diagram, const DiagramStyle& style, qreal availableWidth,
                      const TextMeasurer& measurer)
            : m_diagram(diagram)
            , m_style(style)
            , m_measurer(measurer)
            , m_wrapper(style, measurer)
            , m_availableWidth(availableWidth)
            , m_children(diagram.states.size())
            , m_flow(layouting::LayeredFlowSource { [this](int state, qreal maxWidth)
                                                    { return layoutItem(state, maxWidth); },
                                                    [this](const QList<int>& siblings)
                                                    { return layersOf(siblings); },
                                                    [this](int state, const QList<int>& siblings)
                                                    { return linksAnother(state, siblings); } },
                     style.flowGap)
        {
            for (qsizetype index = 0; index < diagram.states.size(); ++index)
            {
                const int parent = diagram.states.at(index).parent;
                (parent < 0 ? m_roots : m_children[parent]).append(int(index));
            }
            m_labelLimit = std::min(
                style.minLabelWidth,
                std::max<qreal>(0, availableWidth - 2 * style.margin - 2 * style.labelHalo));
            std::map<int, int> labelledLeaving;
            for (const StateTransition& transition : diagram.transitions)
            {
                if (transition.from == transition.to)
                    continue;
                m_tallestLabel
                    = std::max(m_tallestLabel, wrapAt(transition.label, m_labelLimit).height);
                if (!transition.label.isEmpty())
                    m_widestFan = std::max(m_widestFan, ++labelledLeaving[transition.from]);
            }
            setStackedLabels(1);
        }

        Scene layout()
        {
            bool labelsClear = true;
            Scene scene = fitted(labelsClear);
            if (labelsClear || m_widestFan < 2)
                return scene;
            setStackedLabels(std::min(m_widestFan, kMostStackedLabels));
            return fitted(labelsClear);
        }

    private:
        qreal pad() const { return m_style.padding; }

        void setStackedLabels(int stacked)
        {
            m_rowGap = m_style.flowGap + m_tallestLabel * stacked;
            m_flow.setRowGap(m_rowGap);
        }

        Scene fitted(bool& labelsClear) const
        {
            return layouting::narrowedUntilFits(
                [this](qreal contentWidth, bool& clear) { return build(contentWidth, clear); },
                m_availableWidth, m_style.margin, labelsClear);
        }

        WrappedText wrapAt(const QString& text, qreal maxWidth) const
        {
            return m_wrapper.wrapKeepingWords(text, maxWidth);
        }

        qreal textHeight(const WrappedText& text) const { return m_wrapper.heightOf(text); }

        Scene build(qreal contentWidth, bool& labelsClear) const
        {
            m_nestedLabelsClear = true;
            const Fragment flow = m_flow.layoutLevel(m_roots, contentWidth);
            const WrappedText title = m_wrapper.wrap(m_diagram.title, contentWidth);

            qreal top = m_style.margin;
            if (!title.lines.isEmpty())
                top += title.height + m_style.rowGap;
            const qreal flowSpan = flow.reachLeft + flow.width + flow.reachRight;
            const qreal totalWidth = std::max(flowSpan, title.width);
            const qreal originX = m_style.margin + (totalWidth - flowSpan) / 2 + flow.reachLeft;
            const qreal height = layouting::wholePixels(top + flow.height + m_style.margin);

            Fragment placed;
            merge(placed, flow, originX, top);
            const RoutedLevel routed = routeLevel(placed, transitionsScopedTo(-1), height,
                                                  contentWidth + m_style.margin);
            labelsClear = routed.clear && m_nestedLabelsClear;

            const qreal rightmost
                = std::max(originX + flow.width + flow.reachRight, routed.rightmost);
            const qreal overhang = -routed.leftmost;
            const qreal width = layouting::wholePixels(
                std::max(totalWidth + 2 * m_style.margin, rightmost + m_style.margin) + overhang);

            QList<SceneItem> content = placed.items;
            content.append(routed.items);

            Scene scene;
            scene.size = QSizeF(width, height);
            scene.fontFamily = m_style.fontFamily;
            scene.fontPixelSize = m_style.fontPixelSize;
            scene.lineHeight = m_measurer.lineHeight();
            scene.ascent = m_measurer.ascent();
            scene.arrowHeadSize = m_style.arrowHeadSize;
            if (!title.lines.isEmpty())
                scene.items.append(SceneText {
                    QRectF((width - title.width) / 2, m_style.margin, title.width, title.height),
                    Qt::AlignHCenter, title.lines, ColorRole::Text });
            layouting::appendShifted(scene.items, content, overhang, 0);
            return scene;
        }

        struct RoutedLevel
        {
            QList<SceneItem> items;
            QList<QRectF> labelBounds;
            qreal rightmost = 0;
            qreal leftmost = 0;
            bool clear = true;
        };

        RoutedLevel routeLevel(const Fragment& placed, const QList<int>& transitions, qreal height,
                               qreal widthLimit) const
        {
            RoutedLevel roomy = routeLevelWith(placed, transitions, height, true);
            if (roomy.rightmost <= widthLimit + kSameCoordinate)
                return roomy;
            RoutedLevel compact = routeLevelWith(placed, transitions, height, false);
            return compact.rightmost < roomy.rightmost ? compact : roomy;
        }

        RoutedLevel routeLevelWith(const Fragment& placed, const QList<int>& transitions,
                                   qreal height, bool labelsBesideLanes) const
        {
            const EdgeRouter router(m_diagram, placed, m_rowGap, transitions);
            QList<Route> routes = router.directRoutes();
            const QList<WrappedText> labels = labelsOf(transitions);
            QList<QRectF> labelBounds(routes.size());
            QList<qsizetype> directLabels;
            QList<qsizetype> aroundLabels;
            for (qsizetype index = 0; index < labels.size(); ++index)
            {
                if (labels.at(index).lines.isEmpty())
                    continue;
                (router.goesAround(index) ? aroundLabels : directLabels).append(index);
            }
            RoutedLevel routed;
            routed.clear
                = placeLabels(directLabels, routes, labels, placed, height, labelBounds, false);
            QList<QRectF> directBounds;
            for (const QRectF& bounds : std::as_const(labelBounds))
            {
                if (!bounds.isNull())
                    directBounds.append(expanded(bounds));
            }
            QList<qreal> occupiedWidths;
            for (const WrappedText& label : labels)
                occupiedWidths.append(label.lines.isEmpty() || !labelsBesideLanes
                                          ? 0
                                          : label.width + 2 * m_style.labelHalo);
            const QList<Route> aroundRoutes = router.aroundRoutes(directBounds, occupiedWidths);
            for (qsizetype index = 0; index < routes.size(); ++index)
            {
                if (!aroundRoutes.at(index).isEmpty())
                    routes[index] = aroundRoutes.at(index);
            }
            if (!placeLabels(aroundLabels, routes, labels, placed, height, labelBounds,
                             labelsBesideLanes))
                routed.clear = false;

            for (const Route& route : routes)
            {
                for (const QPointF& point : route)
                    routed.rightmost = std::max(routed.rightmost, point.x());
                if (route.size() >= 2)
                    routed.items.append(
                        SceneLine { route, ColorRole::Line, false, ArrowHead::Filled });
            }
            for (qsizetype index = 0; index < labels.size(); ++index)
            {
                const QRectF& bounds = labelBounds.at(index);
                if (bounds.isNull())
                    continue;
                routed.rightmost = std::max(routed.rightmost, bounds.right() + m_style.labelHalo);
                routed.leftmost = std::min(routed.leftmost, bounds.left() - m_style.labelHalo);
                layouting::appendHaloLabel(routed.items, bounds, labels.at(index),
                                           m_style.labelHalo);
                routed.labelBounds.append(
                    bounds.adjusted(-m_style.labelHalo, 0, m_style.labelHalo, 0));
            }
            return routed;
        }

        QList<int> transitionsScopedTo(int scope) const
        {
            QList<int> scoped;
            for (qsizetype index = 0; index < m_diagram.transitions.size(); ++index)
            {
                const StateTransition& transition = m_diagram.transitions.at(index);
                if (scopeOf(transition.from, transition.to) == scope)
                    scoped.append(int(index));
            }
            return scoped;
        }

        int scopeOf(int from, int to) const
        {
            for (int outer = m_diagram.states.at(from).parent; outer >= 0;
                 outer = m_diagram.states.at(outer).parent)
            {
                for (int other = m_diagram.states.at(to).parent; other >= 0;
                     other = m_diagram.states.at(other).parent)
                {
                    if (other == outer)
                        return outer;
                }
            }
            return -1;
        }

        QList<WrappedText> labelsOf(const QList<int>& transitions) const
        {
            QList<WrappedText> labels(m_diagram.transitions.size());
            for (const int index : transitions)
            {
                const StateTransition& transition = m_diagram.transitions.at(index);
                if (transition.from != transition.to)
                    labels[index] = wrapAt(transition.label, m_labelLimit);
            }
            return labels;
        }

        int ancestorIn(int state, const QList<int>& siblings) const
        {
            for (int current = state; current >= 0; current = m_diagram.states.at(current).parent)
            {
                if (siblings.contains(current))
                    return current;
            }
            return -1;
        }

        const StateNote* noteOn(int state, StateNoteSide side) const
        {
            for (const StateNote& note : m_diagram.notes)
            {
                if (note.state == state && note.side == side)
                    return &note;
            }
            return nullptr;
        }

        QList<int> loopsOf(int state) const
        {
            QList<int> loops;
            for (qsizetype index = 0; index < m_diagram.transitions.size(); ++index)
            {
                const StateTransition& transition = m_diagram.transitions.at(index);
                if (transition.from == state && transition.to == state)
                    loops.append(int(index));
            }
            return loops;
        }

        QList<QList<int>> layersOf(const QList<int>& siblings) const
        {
            const qsizetype count = siblings.size();
            if (count == 0)
                return {};
            QList<QList<int>> successors(count);
            for (const StateTransition& transition : m_diagram.transitions)
            {
                const qsizetype from = siblings.indexOf(ancestorIn(transition.from, siblings));
                const qsizetype to = siblings.indexOf(ancestorIn(transition.to, siblings));
                if (from < 0 || to < 0 || from == to || successors[from].contains(int(to)))
                    continue;
                successors[from].append(int(to));
            }

            QList<int> visitOrder;
            for (int node = 0; node < count; ++node)
            {
                if (m_diagram.states.at(siblings.at(node)).kind == StateKind::Initial)
                    visitOrder.append(node);
            }
            for (int node = 0; node < count; ++node)
            {
                if (m_diagram.states.at(siblings.at(node)).kind != StateKind::Initial)
                    visitOrder.append(node);
            }
            const QList<int> layerOf = layouting::longestPathLayers(successors, visitOrder);
            QList<QList<int>> layers = layouting::groupedByLayer(siblings, layerOf);
            for (QList<int>& layer : layers)
            {
                std::stable_partition(
                    layer.begin(), layer.end(), [this](int state)
                    { return m_diagram.states.at(state).kind == StateKind::Final; });
            }
            return layers;
        }

        bool linksAnother(int state, const QList<int>& siblings) const
        {
            for (const StateTransition& transition : m_diagram.transitions)
            {
                const int from = ancestorIn(transition.from, siblings);
                const int to = ancestorIn(transition.to, siblings);
                if ((from == state || to == state) && from >= 0 && to >= 0 && from != to)
                    return true;
            }
            return false;
        }

        QList<LoopLabel> loopLabelsOf(const QList<int>& loops, qreal limit) const
        {
            QList<LoopLabel> labels;
            for (const int loop : loops)
            {
                LoopLabel label;
                label.text = wrapAt(m_diagram.transitions.at(loop).label, limit);
                label.band = std::max(kLoopHeight, label.text.height) + kLoopBandSpacing;
                labels.append(label);
            }
            return labels;
        }

        Fragment layoutItem(int state, qreal maxWidth) const
        {
            const StateNote* left = noteOn(state, StateNoteSide::Left);
            const StateNote* right = noteOn(state, StateNoteSide::Right);
            const QList<int> loops = loopsOf(state);
            if (!left && !right && loops.isEmpty())
                return layoutBody(state, maxWidth, 0);

            const qreal noteGap = m_style.columnGap;
            const bool unbounded = maxWidth == kUnbounded;
            const qreal share = unbounded ? kUnbounded : (maxWidth - noteGap) / 3;
            const QList<LoopLabel> loopLabels = loopLabelsOf(loops, std::min(m_labelLimit, share));
            qreal loopsHeight = 0;
            qreal widestLoopLabel = 0;
            for (const LoopLabel& label : loopLabels)
            {
                loopsHeight += label.band;
                widestLoopLabel = std::max(widestLoopLabel, label.text.width);
            }
            const qreal loopWidth = loops.isEmpty() ? 0
                                                    : m_style.selfLoopWidth
                    + (widestLoopLabel > 0 ? kLoopLabelGap + widestLoopLabel : 0);
            const qreal bodyLimit = unbounded ? kUnbounded : maxWidth - loopWidth;
            const Fragment body = layoutBody(state, bodyLimit, loopsHeight);
            const qreal noteLimit = unbounded
                ? kUnbounded
                : std::max(m_style.minColumnWidth,
                           (maxWidth - loopWidth - body.width) / 2 - noteGap);
            const NoteBox leftBox = left
                ? layouting::noteBoxOf(left->text, noteLimit, m_wrapper, m_style)
                : NoteBox {};
            const NoteBox rightBox = right
                ? layouting::noteBoxOf(right->text, noteLimit, m_wrapper, m_style)
                : NoteBox {};

            Fragment fragment;
            fragment.height = std::max({ body.height, leftBox.height, rightBox.height });
            if (left)
                layouting::placeNote(fragment, leftBox, -noteGap - leftBox.width,
                                     centred(fragment.height - leftBox.height), m_style);
            merge(fragment, body, loopWidth, centred(fragment.height - body.height));
            addLoops(fragment, state, loops, loopLabels, loopsHeight);
            const qreal bodyRight = loopWidth + body.width;
            if (right)
                layouting::placeNote(fragment, rightBox, bodyRight + noteGap,
                                     centred(fragment.height - rightBox.height), m_style);
            fragment.width = bodyRight;
            fragment.reachLeft = left ? noteGap + leftBox.width : 0;
            fragment.reachRight = right ? noteGap + rightBox.width : 0;
            return fragment;
        }

        void addLoops(Fragment& fragment, int state, const QList<int>& loops,
                      const QList<LoopLabel>& labels, qreal loopsHeight) const
        {
            if (loops.isEmpty())
                return;
            const QRectF body = fragment.rects.at(state);
            qreal bandTop = body.center().y() - loopsHeight / 2;
            for (qsizetype index = 0; index < loops.size(); ++index)
            {
                const LoopLabel& label = labels.at(index);
                const qreal centreY = bandTop + label.band / 2;
                const qreal outerX = body.left() - m_style.selfLoopWidth;
                const qreal upperY = centreY - kLoopHeight / 2;
                const qreal lowerY = centreY + kLoopHeight / 2;
                fragment.items.append(
                    SceneLine { { QPointF(body.left(), upperY), QPointF(outerX, upperY),
                                  QPointF(outerX, lowerY), QPointF(body.left(), lowerY) },
                                ColorRole::Line,
                                false,
                                ArrowHead::Filled });
                qreal reachedX = outerX;
                if (!label.text.lines.isEmpty())
                {
                    const QRectF bounds(outerX - kLoopLabelGap - label.text.width,
                                        centreY - label.text.height / 2, label.text.width,
                                        label.text.height);
                    fragment.items.append(SceneText { bounds, Qt::AlignLeft, label.text.lines,
                                                      ColorRole::MutedText });
                    reachedX = bounds.left();
                }
                fragment.obstacles.append(
                    { state, QRectF(reachedX, bandTop, body.left() - reachedX, label.band) });
                bandTop += label.band;
            }
        }

        Fragment layoutBody(int index, qreal maxWidth, qreal minHeight) const
        {
            const State& state = m_diagram.states.at(index);
            if (state.kind != StateKind::Normal)
                return layoutPseudo(index);
            if (state.isComposite)
                return layoutComposite(index, maxWidth, minHeight);
            return layoutLeaf(index, maxWidth, minHeight);
        }

        Fragment layoutPseudo(int index) const
        {
            const qreal radius = m_style.nodeRadius;
            const QRectF rect(0, 0, 2 * radius, 2 * radius);
            Fragment fragment;
            fragment.width = fragment.height = 2 * radius;
            if (m_diagram.states.at(index).kind == StateKind::Initial)
            {
                fragment.items.append(SceneEllipse { rect, ColorRole::Line, std::nullopt });
            }
            else
            {
                const qreal inner = radius * 0.6;
                fragment.items.append(SceneEllipse { rect, std::nullopt, ColorRole::Line });
                fragment.items.append(
                    SceneEllipse { QRectF(radius - inner, radius - inner, 2 * inner, 2 * inner),
                                   ColorRole::Line, std::nullopt });
            }
            fragment.rects[index] = rect;
            fragment.obstacles.append({ index, rect });
            return fragment;
        }

        QList<WrappedText> descriptionsOf(const State& state, qreal limit) const
        {
            QList<WrappedText> descriptions;
            for (const QString& description : state.descriptions)
                descriptions.append(wrapAt(description, limit));
            return descriptions;
        }

        Fragment layoutLeaf(int index, qreal maxWidth, qreal minHeight) const
        {
            const State& state = m_diagram.states.at(index);
            const qreal limit = maxWidth - 2 * pad();
            const WrappedText name = wrapAt(state.label, limit);
            const QList<WrappedText> descriptions = descriptionsOf(state, limit);

            qreal textWidth = name.width;
            for (const WrappedText& description : descriptions)
                textWidth = std::max(textWidth, description.width);
            const qreal nameHeight = textHeight(name);
            qreal contentHeight = nameHeight;
            for (const WrappedText& description : descriptions)
                contentHeight += description.height;
            if (!descriptions.isEmpty())
                contentHeight += pad();

            Fragment fragment;
            fragment.width
                = layouting::wholePixels(std::max(textWidth + 2 * pad(), m_style.minColumnWidth));
            fragment.height
                = layouting::wholePixels(std::max(contentHeight + 2 * pad(), minHeight));
            const QRectF rect(0, 0, fragment.width, fragment.height);
            fragment.items.append(SceneRect { rect, m_style.actionRadius, ColorRole::BoxFill,
                                              ColorRole::BoxStroke, false });

            qreal y = descriptions.isEmpty() ? (fragment.height - nameHeight) / 2 : pad();
            if (!name.lines.isEmpty())
                fragment.items.append(SceneText {
                    QRectF((fragment.width - name.width) / 2, y, name.width, name.height),
                    Qt::AlignHCenter, name.lines, ColorRole::Text });
            if (!descriptions.isEmpty())
            {
                y += nameHeight + pad() / 2;
                fragment.items.append(SceneLine { { QPointF(0, y), QPointF(fragment.width, y) },
                                                  ColorRole::Divider,
                                                  false,
                                                  ArrowHead::None });
                y += pad() / 2;
                appendDescriptions(fragment, descriptions, y);
            }
            fragment.rects[index] = rect;
            fragment.obstacles.append({ index, rect });
            return fragment;
        }

        void appendDescriptions(Fragment& fragment, const QList<WrappedText>& descriptions,
                                qreal top) const
        {
            qreal y = top;
            for (const WrappedText& description : descriptions)
            {
                if (!description.lines.isEmpty())
                    fragment.items.append(
                        SceneText { QRectF(pad(), y, description.width, description.height),
                                    Qt::AlignLeft, description.lines, ColorRole::Text });
                y += description.height;
            }
        }

        Fragment routedContent(int composite, qreal limit) const
        {
            const Fragment content = m_flow.layoutLevel(m_children.at(composite), limit);
            const QList<int> scoped = transitionsScopedTo(composite);
            if (scoped.isEmpty())
                return content;
            const RoutedLevel routed = routeLevel(content, scoped, content.height, limit);
            if (!routed.clear)
                m_nestedLabelsClear = false;
            const qreal overhang = -routed.leftmost;
            Fragment result;
            merge(result, content, overhang, 0);
            layouting::appendShifted(result.items, routed.items, overhang, 0);
            for (const QRectF& bounds : routed.labelBounds)
                result.obstacles.append({ -1, bounds.translated(overhang, 0) });
            result.width = std::max(content.width, routed.rightmost) + overhang;
            result.height = content.height;
            result.reachLeft = std::max<qreal>(0, content.reachLeft - overhang);
            result.reachRight
                = std::max<qreal>(0, overhang + content.width + content.reachRight - result.width);
            return result;
        }

        static qreal spanOf(const Fragment& fragment)
        {
            return fragment.reachLeft + fragment.width + fragment.reachRight;
        }

        Fragment layoutContent(int composite, qreal limit) const
        {
            const Fragment content = routedContent(composite, limit);
            if (limit == kUnbounded || spanOf(content) <= limit + kSameCoordinate)
                return content;
            const qreal narrower = limit - (spanOf(content) - limit);
            if (narrower <= 0)
                return content;
            const Fragment retried = routedContent(composite, narrower);
            return spanOf(retried) < spanOf(content) ? retried : content;
        }

        Fragment layoutComposite(int index, qreal maxWidth, qreal minHeight) const
        {
            const State& state = m_diagram.states.at(index);
            const qreal inset = pad() + m_style.groupInset;
            const qreal limit = maxWidth - 2 * pad();
            const WrappedText label = wrapAt(state.label, limit);
            const QList<WrappedText> descriptions = descriptionsOf(state, limit);

            qreal headerWidth = label.width;
            qreal headerHeight = label.lines.isEmpty() ? 0 : label.height + pad();
            for (const WrappedText& description : descriptions)
            {
                headerWidth = std::max(headerWidth, description.width);
                headerHeight += description.height;
            }
            if (!descriptions.isEmpty())
                headerHeight += pad();
            const Fragment content = layoutContent(index, maxWidth - 2 * inset);
            const qreal contentTop = headerHeight > 0 ? headerHeight + pad() : inset;

            Fragment fragment;
            fragment.width = layouting::wholePixels(std::max(
                { spanOf(content) + 2 * inset, headerWidth + 2 * pad(), m_style.minColumnWidth }));
            fragment.height = layouting::wholePixels(std::max(
                content.height > 0 ? contentTop + content.height + inset : headerHeight + inset,
                minHeight));
            const QRectF frame(0, 0, fragment.width, fragment.height);
            fragment.items.append(SceneRect { frame, m_style.actionRadius, std::nullopt,
                                              ColorRole::GroupStroke, false });
            if (!label.lines.isEmpty())
            {
                const QRectF labelBounds(pad(), pad() / 2, label.width, label.height);
                fragment.items.append(
                    SceneText { labelBounds, Qt::AlignLeft, label.lines, ColorRole::Text });
                fragment.obstacles.append({ index, labelBounds });
            }
            if (!descriptions.isEmpty())
            {
                const qreal dividerY = label.height + pad();
                fragment.items.append(
                    SceneLine { { QPointF(0, dividerY), QPointF(fragment.width, dividerY) },
                                ColorRole::GroupStroke,
                                false,
                                ArrowHead::None });
                appendDescriptions(fragment, descriptions, dividerY + pad() / 2);
            }
            merge(fragment, content, centred(fragment.width - spanOf(content)) + content.reachLeft,
                  contentTop);
            fragment.rects[index] = frame;
            return fragment;
        }

        QRectF expanded(const QRectF& bounds) const
        {
            return bounds.adjusted(-m_style.labelHalo, 0, m_style.labelHalo, 0);
        }

        bool placeLabels(const QList<qsizetype>& indices, const QList<Route>& routes,
                         const QList<WrappedText>& labels, const Fragment& placed, qreal height,
                         QList<QRectF>& bounds, bool besideOuterVertical) const
        {
            layouting::LabelSearch search;
            search.candidatesOf = [&](qsizetype index)
            {
                const WrappedText& label = labels.at(index);
                return layouting::labelCandidatesAlong(
                    routes.at(index), QSizeF(label.width, label.height), besideOuterVertical);
            };
            search.ownRouteOf = [](qsizetype index) { return index; };
            search.routes = &routes;
            search.obstacles = &placed.obstacles;
            search.height = height;
            search.halo = m_style.labelHalo;
            return layouting::placedLabels(search, indices, bounds);
        }

        const StateDiagram& m_diagram;
        const DiagramStyle& m_style;
        const TextMeasurer& m_measurer;
        layouting::TextWrapper m_wrapper;
        qreal m_availableWidth;
        QList<int> m_roots;
        QList<QList<int>> m_children;
        qreal m_rowGap = 0;
        qreal m_labelLimit = 0;
        qreal m_tallestLabel = 0;
        int m_widestFan = 1;
        mutable bool m_nestedLabelsClear = true;
        layouting::LayeredFlow m_flow;
    };
}

Scene StateLayout::layout(const StateDiagram& diagram, const DiagramStyle& style,
                          qreal availableWidth, const TextMeasurer& measurer)
{
    return StateLayouter(diagram, style, availableWidth, measurer).layout();
}
}
