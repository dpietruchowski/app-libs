#include "classlayout.h"

#include "diagram/layoutsupport.h"

#include <algorithm>
#include <cmath>
#include <limits>
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
    using layouting::Obstacle;
    using layouting::Route;
    using layouting::Side;
    using layouting::WrappedText;

    constexpr qreal kUnbounded = std::numeric_limits<qreal>::infinity();
    constexpr qreal kTriangleLength = 12;
    constexpr qreal kTriangleHalfWidth = 5;
    constexpr qreal kDiamondLength = 14;
    constexpr qreal kDiamondHalfWidth = 4.5;
    constexpr qreal kArrowHalfWidthRatio = 0.45;
    constexpr qreal kEndSlack = 2;
    constexpr qreal kMinimumLaneDepth = 16;
    constexpr qreal kPortSpacing = 12;
    constexpr qreal kPortLabelRoom = 4;
    constexpr qreal kEndLabelOffset = 6;
    constexpr qreal kEndLabelStep = 8;
    constexpr int kEndLabelSteps = 4;
    constexpr qreal kBoxWidthShare = 0.6;
    constexpr int kMostRowGapGrowths = 3;

    enum class EdgeKind
    {
        Down,
        Around
    };

    enum class LabelPart
    {
        Middle,
        Upper,
        Lower
    };

    bool marksTheWhole(ClassLinkEnd end)
    {
        return end == ClassLinkEnd::Triangle || end == ClassLinkEnd::FilledDiamond
            || end == ClassLinkEnd::HollowDiamond;
    }

    bool upperIsFrom(const ClassRelation& relation)
    {
        return marksTheWhole(relation.fromEnd) || !marksTheWhole(relation.toEnd);
    }

    qreal markerLength(ClassLinkEnd end, const DiagramStyle& style)
    {
        switch (end)
        {
            case ClassLinkEnd::None:
                return 0;
            case ClassLinkEnd::Triangle:
                return kTriangleLength;
            case ClassLinkEnd::FilledDiamond:
            case ClassLinkEnd::HollowDiamond:
                return kDiamondLength;
            case ClassLinkEnd::Arrow:
                break;
        }
        return style.arrowHeadSize;
    }

    qreal markerHalfWidth(ClassLinkEnd end, const DiagramStyle& style)
    {
        switch (end)
        {
            case ClassLinkEnd::None:
                return 0;
            case ClassLinkEnd::Triangle:
                return kTriangleHalfWidth;
            case ClassLinkEnd::FilledDiamond:
            case ClassLinkEnd::HollowDiamond:
                return kDiamondHalfWidth;
            case ClassLinkEnd::Arrow:
                break;
        }
        return style.arrowHeadSize * kArrowHalfWidthRatio;
    }

    qreal clearanceBeyond(ClassLinkEnd end, const DiagramStyle& style)
    {
        const qreal length = markerLength(end, style);
        return length > 0 ? length + kEndSlack : 0;
    }

    struct Marker
    {
        ClassLinkEnd end = ClassLinkEnd::None;
        QPointF tip;
        QPointF direction;
    };

    QList<QPointF> markerOutline(const Marker& marker, const DiagramStyle& style)
    {
        const qreal length = markerLength(marker.end, style);
        const qreal half = markerHalfWidth(marker.end, style);
        const QPointF normal(-marker.direction.y(), marker.direction.x());
        const QPointF base = marker.tip + marker.direction * length;
        switch (marker.end)
        {
            case ClassLinkEnd::None:
                break;
            case ClassLinkEnd::Triangle:
                return { marker.tip, base + normal * half, base - normal * half };
            case ClassLinkEnd::FilledDiamond:
            case ClassLinkEnd::HollowDiamond:
            {
                const QPointF middle = marker.tip + marker.direction * (length / 2);
                return { marker.tip, middle + normal * half, base, middle - normal * half };
            }
            case ClassLinkEnd::Arrow:
                return { base + normal * half, marker.tip, base - normal * half };
        }
        return {};
    }

    QRectF markerBounds(const Marker& marker, const DiagramStyle& style)
    {
        const QList<QPointF> outline = markerOutline(marker, style);
        qreal left = outline.first().x();
        qreal right = left;
        qreal top = outline.first().y();
        qreal bottom = top;
        for (const QPointF& point : outline)
        {
            left = std::min(left, point.x());
            right = std::max(right, point.x());
            top = std::min(top, point.y());
            bottom = std::max(bottom, point.y());
        }
        return QRectF(QPointF(left, top), QPointF(right, bottom));
    }

    SceneItem markerItem(const Marker& marker, const DiagramStyle& style)
    {
        const QList<QPointF> outline = markerOutline(marker, style);
        switch (marker.end)
        {
            case ClassLinkEnd::FilledDiamond:
                return ScenePolygon { outline, ColorRole::Line, ColorRole::Line };
            case ClassLinkEnd::Triangle:
            case ClassLinkEnd::HollowDiamond:
                return ScenePolygon { outline, ColorRole::Background, ColorRole::Line };
            case ClassLinkEnd::Arrow:
            case ClassLinkEnd::None:
                break;
        }
        return SceneLine { outline, ColorRole::Line, false, ArrowHead::None };
    }

    struct Edge
    {
        int upper = -1;
        int lower = -1;
        QRectF upperRect;
        QRectF lowerRect;
        ClassLinkEnd upperEnd = ClassLinkEnd::None;
        ClassLinkEnd lowerEnd = ClassLinkEnd::None;
        EdgeKind kind = EdgeKind::Down;
        QPointF start;
        QPointF end;
        Route route;
    };

    struct LaneRoom
    {
        qreal middle = 0;
        qreal upperEnd = 0;
        qreal lowerEnd = 0;
    };

    class ClassRouter
    {
    public:
        ClassRouter(const ClassDiagram& diagram, const Fragment& placed, const DiagramStyle& style)
            : m_placed(placed)
            , m_style(style)
        {
            for (const ClassRelation& relation : diagram.relations)
            {
                Edge edge;
                const bool fromIsUpper = upperIsFrom(relation);
                edge.upper = fromIsUpper ? relation.from : relation.to;
                edge.lower = fromIsUpper ? relation.to : relation.from;
                edge.upperEnd = fromIsUpper ? relation.fromEnd : relation.toEnd;
                edge.lowerEnd = fromIsUpper ? relation.toEnd : relation.fromEnd;
                edge.upperRect = m_placed.rects.at(edge.upper);
                edge.lowerRect = m_placed.rects.at(edge.lower);
                decide(edge);
                m_edges.append(edge);
            }
            allocatePorts();
            while (sendBlockedDownEdgesAround())
                allocatePorts();
            routeDown();
        }

        const QList<Edge>& edges() const { return m_edges; }

        bool goesAround(qsizetype edge) const { return m_edges.at(edge).kind == EdgeKind::Around; }

        QList<Route> directRoutes() const
        {
            QList<Route> routes(m_edges.size());
            for (qsizetype index = 0; index < m_edges.size(); ++index)
            {
                if (m_edges.at(index).kind == EdgeKind::Down)
                    routes[index] = m_edges.at(index).route;
            }
            return routes;
        }

        QList<Route> aroundRoutes(const QList<QRectF>& labels, const QList<LaneRoom>& rooms) const
        {
            QList<qsizetype> around;
            QList<layouting::LaneRequest> requests;
            for (qsizetype index = 0; index < m_edges.size(); ++index)
            {
                const Edge& edge = m_edges.at(index);
                if (edge.kind != EdgeKind::Around)
                    continue;
                around.append(index);
                layouting::LaneRequest request;
                request.low = std::min(edge.start.y(), edge.end.y());
                request.high = std::max(edge.start.y(), edge.end.y());
                request.labelWidth = rooms.at(index).middle;
                request.minimumX = std::max(edge.upperRect.right()
                                                + std::max(clearanceBeyond(edge.upperEnd, m_style)
                                                               + rooms.at(index).upperEnd,
                                                           kMinimumLaneDepth),
                                            edge.lowerRect.right()
                                                + std::max(clearanceBeyond(edge.lowerEnd, m_style)
                                                               + rooms.at(index).lowerEnd,
                                                           kMinimumLaneDepth));
                request.route = [&edge](qreal laneX)
                {
                    return layouting::withoutRepeats({ edge.start, QPointF(laneX, edge.start.y()),
                                                       QPointF(laneX, edge.end.y()), edge.end });
                };
                requests.append(request);
            }
            const QList<Route> routed
                = layouting::routedInLanes(requests, [this, &labels](qreal low, qreal high)
                                           { return rightEdgeWithin(low, high, labels); });
            QList<Route> routes(m_edges.size());
            for (qsizetype position = 0; position < around.size(); ++position)
                routes[around.at(position)] = routed.at(position);
            return routes;
        }

        QList<Marker> markers() const
        {
            QList<Marker> markers;
            for (const Edge& edge : m_edges)
            {
                const bool down = edge.kind == EdgeKind::Down;
                if (edge.upperEnd != ClassLinkEnd::None)
                    markers.append(
                        { edge.upperEnd, edge.start, down ? QPointF(0, 1) : QPointF(1, 0) });
                if (edge.lowerEnd != ClassLinkEnd::None)
                    markers.append(
                        { edge.lowerEnd, edge.end, down ? QPointF(0, -1) : QPointF(1, 0) });
            }
            return markers;
        }

    private:
        bool downIsFree(const Edge& edge) const
        {
            const auto [exitX, entryX]
                = alignedX(edge.upperRect, edge.lowerRect, edge.upperRect.center().x(),
                           edge.lowerRect.center().x(), true, true);
            const Route tentative = downRouteVia(QPointF(exitX, edge.upperRect.bottom()),
                                                 QPointF(entryX, edge.lowerRect.top()));
            return layouting::isFree(tentative, m_placed.obstacles, edge.upper, edge.lower);
        }

        void decide(Edge& edge) const
        {
            const bool forward
                = edge.upper != edge.lower && edge.upperRect.bottom() <= edge.lowerRect.top();
            edge.kind = forward && downIsFree(edge) ? EdgeKind::Down : EdgeKind::Around;
        }

        bool sendBlockedDownEdgesAround()
        {
            bool changed = false;
            for (Edge& edge : m_edges)
            {
                if (edge.kind != EdgeKind::Down)
                    continue;
                const Route route = downRouteVia(edge.start, edge.end);
                if (layouting::isFree(route, m_placed.obstacles, edge.upper, edge.lower))
                    continue;
                edge.kind = EdgeKind::Around;
                changed = true;
            }
            return changed;
        }

        void allocatePorts()
        {
            QList<layouting::PortEnd> ends;
            for (qsizetype index = 0; index < m_edges.size(); ++index)
            {
                const Edge& edge = m_edges.at(index);
                const bool down = edge.kind == EdgeKind::Down;
                ends.append({ int(index), true, edge.upper, down ? Side::Bottom : Side::Right,
                              edge.lowerRect });
                ends.append({ int(index), false, edge.lower, down ? Side::Top : Side::Right,
                              edge.upperRect });
            }
            const layouting::PortPlacements placements = layouting::allocatedPorts(
                ends, [this](int box, Side) { return m_placed.rects.at(box); },
                [this](int box) { return m_placed.rects.at(box).center().y(); });
            for (qsizetype index = 0; index < m_edges.size(); ++index)
            {
                Edge& edge = m_edges[index];
                const layouting::PortPlacement& exit = placements.at({ int(index), true });
                const layouting::PortPlacement& entry = placements.at({ int(index), false });
                edge.start = exit.point;
                edge.end = entry.point;
                if (edge.kind != EdgeKind::Down)
                    continue;
                const auto [exitX, entryX]
                    = alignedX(edge.upperRect, edge.lowerRect, edge.start.x(), edge.end.x(),
                               exit.alone, entry.alone);
                edge.start.setX(exitX);
                edge.end.setX(entryX);
            }
        }

        void routeDown()
        {
            QList<layouting::DownEnds> ends;
            QList<qsizetype> indices;
            for (qsizetype index = 0; index < m_edges.size(); ++index)
            {
                const Edge& edge = m_edges.at(index);
                if (edge.kind != EdgeKind::Down)
                    continue;
                layouting::DownEnds down;
                down.owner = edge.upper;
                down.start = edge.start;
                down.end = edge.end;
                down.laneFloor = edge.start.y() + clearanceBeyond(edge.upperEnd, m_style);
                down.laneCeiling = edge.end.y() - clearanceBeyond(edge.lowerEnd, m_style);
                ends.append(down);
                indices.append(index);
            }
            const QList<Route> routes = layouting::routedDown(ends);
            for (qsizetype position = 0; position < indices.size(); ++position)
                m_edges[indices.at(position)].route = routes.at(position);
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
            for (const Obstacle& obstacle : m_placed.obstacles)
                consider(obstacle.rect);
            for (const QRectF& label : labels)
                consider(label);
            return right;
        }

        const Fragment& m_placed;
        const DiagramStyle& m_style;
        QList<Edge> m_edges;
    };

    struct LabelSlot
    {
        qsizetype edge = -1;
        LabelPart part = LabelPart::Middle;
        WrappedText text;
    };

    QList<QRectF> endLabelCandidates(const Route& route, bool atStart, qreal clearance,
                                     const QSizeF& size)
    {
        QList<QRectF> candidates;
        if (route.size() < 2)
            return candidates;
        const QPointF origin = atStart ? route.first() : route.last();
        const QPointF next = atStart ? route.at(1) : route.at(route.size() - 2);
        const bool vertical = std::abs(origin.x() - next.x()) < kSameCoordinate;
        for (int step = 0; step < kEndLabelSteps; ++step)
        {
            const qreal offset = clearance + 2 + step * kEndLabelStep;
            if (vertical)
            {
                const qreal sign = next.y() >= origin.y() ? 1 : -1;
                const qreal edgeY = origin.y() + sign * offset;
                const qreal top = sign > 0 ? edgeY : edgeY - size.height();
                candidates.append(QRectF(QPointF(origin.x() + kEndLabelOffset, top), size));
                candidates.append(
                    QRectF(QPointF(origin.x() - kEndLabelOffset - size.width(), top), size));
                continue;
            }
            const qreal sign = next.x() >= origin.x() ? 1 : -1;
            const qreal edgeX = origin.x() + sign * offset;
            const qreal left = sign > 0 ? edgeX : edgeX - size.width();
            candidates.append(QRectF(QPointF(left, origin.y() - size.height() - 2), size));
            candidates.append(QRectF(QPointF(left, origin.y() + 2), size));
        }
        return candidates;
    }

    class ClassLayouter
    {
    public:
        ClassLayouter(const ClassDiagram& diagram, const DiagramStyle& style, qreal availableWidth,
                      const TextMeasurer& measurer)
            : m_diagram(diagram)
            , m_style(style)
            , m_measurer(measurer)
            , m_wrapper(style, measurer)
            , m_availableWidth(availableWidth)
            , m_flow(layouting::LayeredFlowSource { [this](int type, qreal maxWidth)
                                                    { return layoutItem(type, maxWidth); },
                                                    [this](const QList<int>& siblings)
                                                    { return layersOf(siblings); },
                                                    [this](int type, const QList<int>& siblings)
                                                    { return linksAnother(type, siblings); } },
                     style.flowGap)
        {
            for (qsizetype index = 0; index < diagram.types.size(); ++index)
                m_all.append(int(index));
            const qreal contentWidth = std::max<qreal>(0, availableWidth - 2 * style.margin);
            m_labelLimit = std::min(style.minLabelWidth,
                                    std::max<qreal>(0, contentWidth - 2 * style.labelHalo));
            m_boxLimit = std::max(style.minLabelWidth, kBoxWidthShare * contentWidth);
            reservePortRoom();
            for (const ClassRelation& relation : diagram.relations)
            {
                if (relation.from == relation.to)
                    continue;
                m_markerRoom = std::max(m_markerRoom,
                                        markerLength(relation.fromEnd, style)
                                            + markerLength(relation.toEnd, style));
                for (const QString& text :
                     { relation.label, relation.fromMultiplicity, relation.toMultiplicity })
                    m_tallestLabel = std::max(m_tallestLabel, wrapAt(text, m_labelLimit).height);
            }
        }

        Scene layout()
        {
            Scene scene;
            for (int growth = 0; growth <= kMostRowGapGrowths; ++growth)
            {
                m_rowGap = m_style.flowGap + m_markerRoom + m_tallestLabel
                    + qreal(growth) * m_measurer.lineHeight();
                m_flow.setRowGap(m_rowGap);
                bool labelsClear = true;
                scene = fitted(labelsClear);
                if (labelsClear)
                    break;
            }
            return scene;
        }

    private:
        qreal pad() const { return m_style.padding; }

        qreal portSpacing(const QString& multiplicity) const
        {
            if (multiplicity.isEmpty())
                return kPortSpacing;
            return std::max(kPortSpacing, m_measurer.lineHeight() + kPortLabelRoom);
        }

        void reservePortRoom()
        {
            const qsizetype count = m_diagram.types.size();
            m_minWidths = QList<qreal>(count, 0);
            m_minHeights = QList<qreal>(count, 0);
            QList<int> layerOf(count, 0);
            const QList<QList<int>> layers = layersOf(m_all);
            for (qsizetype layer = 0; layer < layers.size(); ++layer)
            {
                for (const int type : layers.at(layer))
                    layerOf[type] = int(layer);
            }
            QList<int> topEnds(count, 0);
            QList<int> bottomEnds(count, 0);
            QList<int> sideEnds(count, 0);
            QList<qreal> sideSpacing(count, kPortSpacing);
            for (const ClassRelation& relation : m_diagram.relations)
            {
                const bool fromIsUpper = upperIsFrom(relation);
                const int upper = fromIsUpper ? relation.from : relation.to;
                const int lower = fromIsUpper ? relation.to : relation.from;
                const QString& upperMultiplicity
                    = fromIsUpper ? relation.fromMultiplicity : relation.toMultiplicity;
                const QString& lowerMultiplicity
                    = fromIsUpper ? relation.toMultiplicity : relation.fromMultiplicity;
                const int distance = layerOf.at(lower) - layerOf.at(upper);
                if (upper == lower || distance != 1)
                {
                    ++sideEnds[upper];
                    ++sideEnds[lower];
                    sideSpacing[upper]
                        = std::max(sideSpacing.at(upper), portSpacing(upperMultiplicity));
                    sideSpacing[lower]
                        = std::max(sideSpacing.at(lower), portSpacing(lowerMultiplicity));
                }
                if (upper != lower && distance >= 1)
                {
                    ++bottomEnds[upper];
                    ++topEnds[lower];
                }
            }
            for (qsizetype type = 0; type < count; ++type)
            {
                m_minHeights[type] = qreal(sideEnds.at(type) + 1) * sideSpacing.at(type);
                const int ends = std::max(topEnds.at(type), bottomEnds.at(type));
                m_minWidths[type] = ends > 0 ? qreal(ends + 1) * kPortSpacing : 0;
            }
        }

        Scene fitted(bool& labelsClear) const
        {
            return layouting::narrowedUntilFits([this](qreal contentWidth, bool& clear)
                                                { return build(contentWidth, clear); },
                                                m_availableWidth, m_style.margin, labelsClear);
        }

        WrappedText wrapAt(const QString& text, qreal maxWidth) const
        {
            return m_wrapper.wrapKeepingWords(text, maxWidth);
        }

        qreal textHeight(const WrappedText& text) const { return m_wrapper.heightOf(text); }

        struct RoutedLevel
        {
            QList<SceneItem> items;
            qreal rightmost = 0;
            qreal leftmost = 0;
            bool clear = true;
        };

        Scene build(qreal contentWidth, bool& labelsClear) const
        {
            const Fragment flow = m_flow.layoutLevel(m_all, contentWidth);
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
            const RoutedLevel routed = routeLevel(placed, height, contentWidth + m_style.margin);
            labelsClear = routed.clear;

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

        RoutedLevel routeLevel(const Fragment& placed, qreal height, qreal widthLimit) const
        {
            RoutedLevel roomy = routeLevelWith(placed, height, true);
            if (roomy.rightmost <= widthLimit + kSameCoordinate)
                return roomy;
            RoutedLevel compact = routeLevelWith(placed, height, false);
            return compact.rightmost < roomy.rightmost ? compact : roomy;
        }

        QList<LabelSlot> slotsOf() const
        {
            QList<LabelSlot> labelSlots;
            for (qsizetype index = 0; index < m_diagram.relations.size(); ++index)
            {
                const ClassRelation& relation = m_diagram.relations.at(index);
                const bool fromIsUpper = upperIsFrom(relation);
                const QString& upper
                    = fromIsUpper ? relation.fromMultiplicity : relation.toMultiplicity;
                const QString& lower
                    = fromIsUpper ? relation.toMultiplicity : relation.fromMultiplicity;
                const std::pair<LabelPart, QString> parts[]
                    = { { LabelPart::Middle, relation.label },
                        { LabelPart::Upper, upper },
                        { LabelPart::Lower, lower } };
                for (const auto& [part, text] : parts)
                {
                    const WrappedText wrapped = wrapAt(text, m_labelLimit);
                    if (!wrapped.lines.isEmpty())
                        labelSlots.append({ index, part, wrapped });
                }
            }
            return labelSlots;
        }

        bool placeSlots(const QList<qsizetype>& indices, const QList<LabelSlot>& labelSlots,
                        const QList<Edge>& edges, const QList<Route>& routes,
                        const QList<Obstacle>& obstacles, const QList<bool>& besideLanes,
                        qreal height, QList<QRectF>& bounds) const
        {
            layouting::LabelSearch search;
            search.candidatesOf = [&](qsizetype index)
            {
                const LabelSlot& slot = labelSlots.at(index);
                const Route& route = routes.at(slot.edge);
                const QSizeF size(slot.text.width, slot.text.height);
                const Edge& edge = edges.at(slot.edge);
                switch (slot.part)
                {
                    case LabelPart::Middle:
                        return layouting::labelCandidatesAlong(route, size,
                                                               besideLanes.at(slot.edge));
                    case LabelPart::Upper:
                        return endLabelCandidates(route, true,
                                                  clearanceBeyond(edge.upperEnd, m_style), size);
                    case LabelPart::Lower:
                        break;
                }
                return endLabelCandidates(route, false, clearanceBeyond(edge.lowerEnd, m_style),
                                          size);
            };
            search.ownRouteOf = [&](qsizetype index) { return labelSlots.at(index).edge; };
            search.routes = &routes;
            search.obstacles = &obstacles;
            search.height = height;
            search.halo = m_style.labelHalo;
            return layouting::placedLabels(search, indices, bounds);
        }

        QRectF expanded(const QRectF& bounds) const
        {
            return bounds.adjusted(-m_style.labelHalo, 0, m_style.labelHalo, 0);
        }

        RoutedLevel routeLevelWith(const Fragment& placed, qreal height,
                                   bool labelsBesideLanes) const
        {
            const ClassRouter router(m_diagram, placed, m_style);
            const QList<Edge>& edges = router.edges();
            QList<Route> routes = router.directRoutes();
            const QList<LabelSlot> labelSlots = slotsOf();

            QList<Obstacle> obstacles = placed.obstacles;
            const QList<Marker> markers = router.markers();
            for (const Marker& marker : markers)
                obstacles.append({ -1, markerBounds(marker, m_style) });

            QList<bool> besideLanes;
            for (qsizetype index = 0; index < edges.size(); ++index)
                besideLanes.append(labelsBesideLanes && router.goesAround(index));

            QList<qsizetype> directSlots;
            QList<qsizetype> aroundSlots;
            for (qsizetype index = 0; index < labelSlots.size(); ++index)
                (router.goesAround(labelSlots.at(index).edge) ? aroundSlots : directSlots)
                    .append(index);

            QList<QRectF> bounds(labelSlots.size());
            RoutedLevel routed;
            routed.clear = placeSlots(directSlots, labelSlots, edges, routes, obstacles,
                                      besideLanes, height, bounds);

            QList<QRectF> directBounds;
            for (const QRectF& rect : std::as_const(bounds))
            {
                if (!rect.isNull())
                    directBounds.append(expanded(rect));
            }
            QList<LaneRoom> rooms(edges.size());
            for (const LabelSlot& slot : labelSlots)
            {
                const qreal room = slot.text.width + 2 * m_style.labelHalo;
                LaneRoom& entry = rooms[slot.edge];
                if (slot.part == LabelPart::Middle)
                    entry.middle = labelsBesideLanes ? room : 0;
                else if (slot.part == LabelPart::Upper)
                    entry.upperEnd = room + kEndSlack;
                else
                    entry.lowerEnd = room + kEndSlack;
            }
            const QList<Route> aroundRoutes = router.aroundRoutes(directBounds, rooms);
            for (qsizetype index = 0; index < routes.size(); ++index)
            {
                if (!aroundRoutes.at(index).isEmpty())
                    routes[index] = aroundRoutes.at(index);
            }
            if (!placeSlots(aroundSlots, labelSlots, edges, routes, obstacles, besideLanes, height,
                            bounds))
                routed.clear = false;

            for (qsizetype index = 0; index < routes.size(); ++index)
            {
                const Route& route = routes.at(index);
                for (const QPointF& point : route)
                    routed.rightmost = std::max(routed.rightmost, point.x());
                if (route.size() >= 2)
                    routed.items.append(
                        SceneLine { route, ColorRole::Line,
                                    m_diagram.relations.at(index).line == ClassLinkLine::Dashed,
                                    ArrowHead::None });
            }
            for (const Marker& marker : markers)
                routed.items.append(markerItem(marker, m_style));
            for (qsizetype index = 0; index < labelSlots.size(); ++index)
            {
                const QRectF& rect = bounds.at(index);
                if (rect.isNull())
                    continue;
                routed.rightmost = std::max(routed.rightmost, rect.right() + m_style.labelHalo);
                routed.leftmost = std::min(routed.leftmost, rect.left() - m_style.labelHalo);
                layouting::appendHaloLabel(routed.items, rect, labelSlots.at(index).text,
                                           m_style.labelHalo);
            }
            return routed;
        }

        QList<QList<int>> layersOf(const QList<int>& siblings) const
        {
            const qsizetype count = siblings.size();
            if (count == 0)
                return {};
            QList<QList<int>> successors(count);
            QList<bool> hasIncoming(count, false);
            for (const ClassRelation& relation : m_diagram.relations)
            {
                if (relation.from == relation.to)
                    continue;
                const bool fromIsUpper = upperIsFrom(relation);
                const qsizetype upper = siblings.indexOf(fromIsUpper ? relation.from : relation.to);
                const qsizetype lower = siblings.indexOf(fromIsUpper ? relation.to : relation.from);
                if (upper < 0 || lower < 0 || successors[upper].contains(int(lower)))
                    continue;
                successors[upper].append(int(lower));
                hasIncoming[lower] = true;
            }
            QList<int> visitOrder;
            for (int node = 0; node < count; ++node)
            {
                if (!hasIncoming.at(node))
                    visitOrder.append(node);
            }
            for (int node = 0; node < count; ++node)
            {
                if (hasIncoming.at(node))
                    visitOrder.append(node);
            }
            return layouting::groupedByLayer(siblings,
                                             layouting::longestPathLayers(successors, visitOrder));
        }

        bool linksAnother(int type, const QList<int>& siblings) const
        {
            for (const ClassRelation& relation : m_diagram.relations)
            {
                if (relation.from == relation.to || !siblings.contains(relation.from)
                    || !siblings.contains(relation.to))
                    continue;
                if (relation.from == type || relation.to == type)
                    return true;
            }
            return false;
        }

        const ClassNote* noteOn(int type, ClassNoteSide side) const
        {
            for (const ClassNote& note : m_diagram.notes)
            {
                if (note.type == type && note.side == side)
                    return &note;
            }
            return nullptr;
        }

        Fragment layoutItem(int type, qreal maxWidth) const
        {
            const ClassNote* left = noteOn(type, ClassNoteSide::Left);
            const ClassNote* right = noteOn(type, ClassNoteSide::Right);
            if (!left && !right)
                return layoutBox(type, maxWidth);

            const qreal noteGap = m_style.columnGap;
            const bool unbounded = maxWidth == kUnbounded;
            const Fragment body = layoutBox(type, maxWidth);
            const qreal noteLimit = unbounded
                ? kUnbounded
                : std::max(m_style.minColumnWidth, (maxWidth - body.width) / 2 - noteGap);
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
            merge(fragment, body, 0, centred(fragment.height - body.height));
            if (right)
                layouting::placeNote(fragment, rightBox, body.width + noteGap,
                                     centred(fragment.height - rightBox.height), m_style);
            fragment.width = body.width;
            fragment.reachLeft = left ? noteGap + leftBox.width : 0;
            fragment.reachRight = right ? noteGap + rightBox.width : 0;
            return fragment;
        }

        static QString kindLabelOf(ClassKind kind)
        {
            switch (kind)
            {
                case ClassKind::Class:
                    break;
                case ClassKind::Abstract:
                    return QStringLiteral("«") + QStringLiteral("abstract") + QStringLiteral("»");
                case ClassKind::Struct:
                    return QStringLiteral("«struct»");
                case ClassKind::Enum:
                    return QStringLiteral("«enum»");
                case ClassKind::Interface:
                    return QStringLiteral("«interface»");
            }
            return QString();
        }

        Fragment layoutBox(int index, qreal maxWidth) const
        {
            const ClassType& type = m_diagram.types.at(index);
            const qreal limit = std::min(m_boxLimit, maxWidth) - 2 * pad();
            const WrappedText name = wrapAt(type.label, limit);
            const QString kind = kindLabelOf(type.kind);
            const WrappedText kindText = kind.isEmpty() ? WrappedText {} : wrapAt(kind, limit);

            QList<QList<WrappedText>> compartments;
            qreal textWidth = std::max(name.width, kindText.width);
            qreal bodyHeight = 0;
            for (const QStringList& members : type.compartments)
            {
                QList<WrappedText> wrapped;
                bodyHeight += pad();
                for (const QString& member : members)
                {
                    wrapped.append(wrapAt(member, limit));
                    textWidth = std::max(textWidth, wrapped.last().width);
                    bodyHeight += textHeight(wrapped.last());
                }
                compartments.append(wrapped);
            }
            const qreal kindHeight = kindText.lines.isEmpty() ? 0 : textHeight(kindText);
            const qreal headerHeight = pad() + kindHeight + textHeight(name) + pad();

            Fragment fragment;
            fragment.width = layouting::wholePixels(
                std::max({ textWidth + 2 * pad(), m_style.minColumnWidth, m_minWidths.at(index) }));
            fragment.height = layouting::wholePixels(
                std::max(headerHeight + bodyHeight, m_minHeights.at(index)));
            const QRectF rect(0, 0, fragment.width, fragment.height);
            fragment.items.append(SceneRect { rect, m_style.boxRadius, ColorRole::BoxFill,
                                              ColorRole::BoxStroke, false });

            qreal y = pad();
            if (!kindText.lines.isEmpty())
            {
                fragment.items.append(SceneText { QRectF((fragment.width - kindText.width) / 2, y,
                                                         kindText.width, kindText.height),
                                                  Qt::AlignHCenter, kindText.lines,
                                                  ColorRole::MutedText });
                y += kindHeight;
            }
            fragment.items.append(
                SceneText { QRectF((fragment.width - name.width) / 2, y, name.width, name.height),
                            Qt::AlignHCenter, name.lines, ColorRole::Text });
            y = headerHeight;
            for (const QList<WrappedText>& members : std::as_const(compartments))
            {
                fragment.items.append(SceneLine { { QPointF(0, y), QPointF(fragment.width, y) },
                                                  ColorRole::Divider,
                                                  false,
                                                  ArrowHead::None });
                y += pad() / 2;
                for (const WrappedText& member : members)
                {
                    if (!member.lines.isEmpty())
                        fragment.items.append(
                            SceneText { QRectF(pad(), y, member.width, member.height),
                                        Qt::AlignLeft, member.lines, ColorRole::Text });
                    y += textHeight(member);
                }
                y += pad() / 2;
            }
            fragment.rects[index] = rect;
            fragment.obstacles.append({ index, rect });
            return fragment;
        }

        const ClassDiagram& m_diagram;
        const DiagramStyle& m_style;
        const TextMeasurer& m_measurer;
        layouting::TextWrapper m_wrapper;
        qreal m_availableWidth;
        QList<int> m_all;
        QList<qreal> m_minWidths;
        QList<qreal> m_minHeights;
        qreal m_rowGap = 0;
        qreal m_labelLimit = 0;
        qreal m_boxLimit = 0;
        qreal m_markerRoom = 0;
        qreal m_tallestLabel = 0;
        layouting::LayeredFlow m_flow;
    };
}

Scene ClassLayout::layout(const ClassDiagram& diagram, const DiagramStyle& style,
                          qreal availableWidth, const TextMeasurer& measurer)
{
    return ClassLayouter(diagram, style, availableWidth, measurer).layout();
}
}
