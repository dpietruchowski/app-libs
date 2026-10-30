#include "layoutsupport.h"

#include <QHash>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <variant>

namespace diagram::layouting
{
namespace
{
    constexpr qreal kUnbounded = std::numeric_limits<qreal>::infinity();
    constexpr qreal kFitTolerance = 0.001;
    constexpr qreal kObstacleClearance = 3;
}

qreal wholePixels(qreal value) { return std::ceil(value - 0.001); }

qreal centred(qreal space) { return std::round(space / 2); }

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

WrappedText TextWrapper::wrapKeepingWords(const QString& text, qreal maxWidth) const
{
    const qreal wordLimit = std::min(widestWord(text), m_style.minLabelWidth);
    return wrap(text, std::max(maxWidth, wordLimit));
}

qreal TextWrapper::heightOf(const WrappedText& text) const
{
    return std::max(text.height, m_measurer.lineHeight());
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

void merge(Fragment& target, const Fragment& part, qreal dx, qreal dy)
{
    appendShifted(target.items, part.items, dx, dy);
    for (const auto& [block, rect] : part.rects)
        target.rects[block] = rect.translated(dx, dy);
    for (const Obstacle& obstacle : part.obstacles)
        target.obstacles.append({ obstacle.block, obstacle.rect.translated(dx, dy) });
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

namespace
{
    bool crosses(const QPointF& from, const QPointF& to, const QRectF& obstacle)
    {
        const QRectF box = obstacle.adjusted(-kObstacleClearance, -kObstacleClearance,
                                             kObstacleClearance, kObstacleClearance);
        return std::min(from.x(), to.x()) <= box.right() && std::max(from.x(), to.x()) >= box.left()
            && std::min(from.y(), to.y()) <= box.bottom()
            && std::max(from.y(), to.y()) >= box.top();
    }
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

QRectF keptInside(QRectF bounds, const QSizeF& size, qreal halo)
{
    const qreal left
        = std::max(halo, std::min(bounds.left(), size.width() - bounds.width() - halo));
    const qreal top = std::max(0.0, std::min(bounds.top(), size.height() - bounds.height()));
    bounds.moveTopLeft(QPointF(left, top));
    return bounds;
}

void appendHaloLabel(QList<SceneItem>& items, const QRectF& bounds, const WrappedText& label,
                     qreal halo)
{
    items.append(SceneRect { bounds.adjusted(-halo, 0, halo, 0), 0, ColorRole::Background,
                             std::nullopt, false });
    items.append(SceneText { bounds, Qt::AlignLeft, label.lines, ColorRole::MutedText });
}

NoteBox noteBoxOf(const QString& text, qreal limit, const TextWrapper& wrapper,
                  const DiagramStyle& style)
{
    NoteBox box;
    box.text = wrapper.wrapKeepingWords(text, limit - 2 * style.padding);
    box.width = box.text.width + 2 * style.padding;
    box.height = wrapper.heightOf(box.text) + 2 * style.padding;
    return box;
}

void placeNote(Fragment& fragment, const NoteBox& box, qreal x, qreal y, const DiagramStyle& style)
{
    const QRectF rect(x, y, box.width, box.height);
    fragment.items.append(SceneRect { rect, 0, ColorRole::NoteFill, ColorRole::NoteStroke, false });
    if (!box.text.lines.isEmpty())
        fragment.items.append(SceneText {
            QRectF(x + style.padding, y + style.padding, box.text.width, box.text.height),
            Qt::AlignLeft, box.text.lines, ColorRole::Text });
    fragment.obstacles.append({ -1, rect });
}

namespace
{
    constexpr qreal kPortMargin = 6;
    constexpr qreal kLaneStep = 5;
    constexpr qreal kLabelHit = 1;
    constexpr int kHardPenalty = 1000;
    constexpr int kOverhangPenalty = 2;
    constexpr int kCrossedLinePenalty = 1;
    constexpr int kRefinePasses = 3;
    constexpr int kMostNarrowerRetries = 4;
    constexpr qsizetype kMostPermutedLanes = 6;
    constexpr qreal kLabelFractions[] = { 0.5, 0.35, 0.65, 0.2, 0.8 };

    bool withinSpan(qreal x, const QRectF& rect)
    {
        return x >= rect.left() + kPortMargin && x <= rect.right() - kPortMargin;
    }

    struct PortUse
    {
        qsizetype end = -1;
        bool isExit = false;
        qreal key = 0;
        qreal tie = 0;
    };

    void orderPortUses(QList<PortUse>& uses, Side side, qreal ownCentre)
    {
        const bool onASide = side == Side::Left || side == Side::Right;
        std::stable_sort(uses.begin(), uses.end(),
                         [&](const PortUse& a, const PortUse& b)
                         {
                             if (!onASide)
                                 return a.key < b.key;
                             const bool aBelow = a.key >= ownCentre;
                             const bool bBelow = b.key >= ownCentre;
                             if (aBelow != bBelow)
                                 return bBelow;
                             if (std::abs(a.key - b.key) < kSameCoordinate)
                             {
                                 if (std::abs(a.tie - b.tie) < kSameCoordinate)
                                     return !a.isExit && b.isExit;
                                 return a.tie > b.tie;
                             }
                             return a.key > b.key;
                         });
    }
}

Route withoutRepeats(const Route& route)
{
    Route cleaned;
    for (const QPointF& point : route)
    {
        if (cleaned.isEmpty() || cleaned.last() != point)
            cleaned.append(point);
    }
    return cleaned;
}

Route downRouteVia(const QPointF& start, const QPointF& end, qreal laneOffset, qreal laneFloor,
                   qreal laneCeiling)
{
    if (std::abs(start.x() - end.x()) < kSameCoordinate)
        return { start, end };
    const qreal middle = std::round((start.y() + end.y()) / 2 + laneOffset);
    const qreal laneY = std::max(laneFloor, std::min(laneCeiling, middle));
    return { start, QPointF(start.x(), laneY), QPointF(end.x(), laneY), end };
}

std::pair<qreal, qreal> alignedX(const QRectF& from, const QRectF& to, qreal exitX, qreal entryX,
                                 bool exitAlone, bool entryAlone)
{
    if (std::abs(exitX - entryX) < kSameCoordinate)
        return { exitX, entryX };
    if (entryAlone && withinSpan(exitX, to))
        return { exitX, exitX };
    if (exitAlone && withinSpan(entryX, from))
        return { entryX, entryX };
    if (exitAlone && entryAlone)
    {
        const qreal low = std::max(from.left(), to.left()) + kPortMargin;
        const qreal high = std::min(from.right(), to.right()) - kPortMargin;
        if (low <= high)
        {
            const qreal shared = std::round((low + high) / 2);
            return { shared, shared };
        }
    }
    return { exitX, entryX };
}

QPointF portAt(const QRectF& rect, Side side, qreal fraction)
{
    switch (side)
    {
        case Side::Top:
            return QPointF(std::round(rect.left() + rect.width() * fraction), rect.top());
        case Side::Bottom:
            return QPointF(std::round(rect.left() + rect.width() * fraction), rect.bottom());
        case Side::Left:
            return QPointF(rect.left(), std::round(rect.top() + rect.height() * fraction));
        case Side::Right:
            break;
    }
    return QPointF(rect.right(), std::round(rect.top() + rect.height() * fraction));
}

qreal portKeyFor(Side side, const QRectF& other)
{
    return side == Side::Top || side == Side::Bottom ? other.center().x() : other.center().y();
}

PortPlacements allocatedPorts(const QList<PortEnd>& ends,
                              const std::function<QRectF(int box, Side side)>& rectOf,
                              const std::function<qreal(int box)>& centreYOf)
{
    std::map<std::pair<int, int>, QList<PortUse>> table;
    for (qsizetype index = 0; index < ends.size(); ++index)
    {
        const PortEnd& end = ends.at(index);
        table[{ end.box, int(end.side) }].append(
            { index, end.isExit, portKeyFor(end.side, end.other), end.other.center().x() });
    }
    PortPlacements placements;
    for (auto& [key, uses] : table)
    {
        const Side side = Side(key.second);
        orderPortUses(uses, side, centreYOf(key.first));
        const QRectF rect = rectOf(key.first, side);
        for (qsizetype position = 0; position < uses.size(); ++position)
        {
            const qreal fraction = qreal(position + 1) / qreal(uses.size() + 1);
            const PortEnd& end = ends.at(uses.at(position).end);
            placements[{ end.edge, end.isExit }] = { portAt(rect, side, fraction),
                                                     uses.size() == 1 };
        }
    }
    return placements;
}

QList<Route> routedDown(const QList<DownEnds>& ends)
{
    QList<Route> routes(ends.size());
    std::map<std::pair<int, int>, QList<qsizetype>> jogging;
    for (qsizetype index = 0; index < ends.size(); ++index)
    {
        const DownEnds& edge = ends.at(index);
        if (std::abs(edge.start.x() - edge.end.x()) < kSameCoordinate)
        {
            routes[index]
                = downRouteVia(edge.start, edge.end, 0, edge.laneFloor, edge.laneCeiling);
            continue;
        }
        const int direction = edge.end.x() < edge.start.x() ? -1 : 1;
        jogging[{ edge.owner, direction }].append(index);
    }
    for (auto& [key, indices] : jogging)
    {
        const int direction = key.second;
        std::stable_sort(indices.begin(), indices.end(),
                         [&](qsizetype a, qsizetype b)
                         {
                             return direction < 0 ? ends.at(a).start.x() < ends.at(b).start.x()
                                                  : ends.at(a).start.x() > ends.at(b).start.x();
                         });
        for (qsizetype position = 0; position < indices.size(); ++position)
        {
            const DownEnds& edge = ends.at(indices.at(position));
            const qreal offset = (qreal(position) - qreal(indices.size() - 1) / 2) * kLaneStep;
            routes[indices.at(position)]
                = downRouteVia(edge.start, edge.end, offset, edge.laneFloor, edge.laneCeiling);
        }
    }
    return routes;
}

int crossingCount(const Route& first, const Route& second)
{
    const auto strictlyBetween = [](qreal value, qreal a, qreal b)
    {
        return value > std::min(a, b) + kSameCoordinate
            && value < std::max(a, b) - kSameCoordinate;
    };
    const auto isVertical
        = [](const QPointF& a, const QPointF& b) { return std::abs(a.x() - b.x()) < kSameCoordinate; };
    int count = 0;
    for (qsizetype i = 1; i < first.size(); ++i)
    {
        for (qsizetype j = 1; j < second.size(); ++j)
        {
            const bool firstVertical = isVertical(first.at(i - 1), first.at(i));
            if (firstVertical == isVertical(second.at(j - 1), second.at(j)))
                continue;
            const QPointF& v0 = firstVertical ? first.at(i - 1) : second.at(j - 1);
            const QPointF& v1 = firstVertical ? first.at(i) : second.at(j);
            const QPointF& h0 = firstVertical ? second.at(j - 1) : first.at(i - 1);
            const QPointF& h1 = firstVertical ? second.at(j) : first.at(i);
            if (strictlyBetween(v0.x(), h0.x(), h1.x()) && strictlyBetween(h0.y(), v0.y(), v1.y()))
                ++count;
        }
    }
    return count;
}

bool routeTouches(const QRectF& bounds, const Route& route)
{
    for (qsizetype index = 1; index < route.size(); ++index)
    {
        const QRectF segment = QRectF(route.at(index - 1), route.at(index))
                                   .normalized()
                                   .adjusted(-0.5, -0.5, 0.5, 0.5);
        if (bounds.intersects(segment))
            return true;
    }
    return false;
}

namespace
{
    QList<Route> laneRoutes(const QList<LaneRequest>& requests, const QList<qsizetype>& order,
                            const std::function<qreal(qreal, qreal)>& rightEdgeWithin)
    {
        QList<Route> routes(requests.size());
        qreal nextFree = 0;
        for (const qsizetype index : order)
        {
            const LaneRequest& request = requests.at(index);
            const qreal laneX = std::max({ rightEdgeWithin(request.low, request.high)
                                               + kLaneClearance,
                                           nextFree, request.minimumX });
            routes[index] = request.route(laneX);
            nextFree = laneX + kLaneSpacing
                + (request.labelWidth > 0 ? kLabelOffset + request.labelWidth : 0);
        }
        return routes;
    }

    int crossingsAmong(const QList<Route>& routes, const QList<qsizetype>& order)
    {
        int count = 0;
        for (qsizetype i = 0; i < order.size(); ++i)
        {
            for (qsizetype j = i + 1; j < order.size(); ++j)
                count += crossingCount(routes.at(order.at(i)), routes.at(order.at(j)));
        }
        return count;
    }
}

QList<Route> routedInLanes(const QList<LaneRequest>& requests,
                           const std::function<qreal(qreal low, qreal high)>& rightEdgeWithin)
{
    QList<qsizetype> around;
    for (qsizetype index = 0; index < requests.size(); ++index)
        around.append(index);
    std::stable_sort(around.begin(), around.end(),
                     [&](qsizetype a, qsizetype b)
                     {
                         return requests.at(a).high - requests.at(a).low
                             < requests.at(b).high - requests.at(b).low;
                     });
    QList<Route> routes = laneRoutes(requests, around, rightEdgeWithin);
    int fewest = crossingsAmong(routes, around);
    if (fewest == 0 || around.size() > kMostPermutedLanes)
        return routes;
    QList<qsizetype> best = around;
    QList<qsizetype> candidate = around;
    std::sort(candidate.begin(), candidate.end());
    do
    {
        const QList<Route> attempt = laneRoutes(requests, candidate, rightEdgeWithin);
        const int count = crossingsAmong(attempt, around);
        if (count < fewest)
        {
            fewest = count;
            best = candidate;
        }
    } while (fewest > 0 && std::next_permutation(candidate.begin(), candidate.end()));
    return laneRoutes(requests, best, rightEdgeWithin);
}

QList<QRectF> labelCandidatesAlong(const Route& route, const QSizeF& size, bool besideOuterVertical)
{
    QList<QRectF> candidates;
    if (besideOuterVertical)
    {
        qsizetype outermost = -1;
        for (qsizetype index = 1; index < route.size(); ++index)
        {
            if (std::abs(route.at(index).x() - route.at(index - 1).x()) >= kSameCoordinate)
                continue;
            if (outermost < 0 || route.at(index).x() > route.at(outermost).x() + kSameCoordinate
                || (std::abs(route.at(index).x() - route.at(outermost).x()) < kSameCoordinate
                    && std::abs(route.at(index).y() - route.at(index - 1).y())
                        > std::abs(route.at(outermost).y() - route.at(outermost - 1).y())))
                outermost = index;
        }
        if (outermost >= 0)
        {
            const QPointF a = route.at(outermost - 1);
            const QPointF b = route.at(outermost);
            for (const qreal fraction : kLabelFractions)
            {
                const qreal centreY = a.y() + (b.y() - a.y()) * fraction;
                candidates.append(
                    QRectF(QPointF(a.x() + kLabelOffset, centreY - size.height() / 2), size));
            }
        }
    }

    QList<qsizetype> order;
    for (qsizetype index = 1; index < route.size(); ++index)
        order.append(index);
    const auto length = [&](qsizetype index)
    {
        const QPointF delta = route.at(index) - route.at(index - 1);
        return std::abs(delta.x()) + std::abs(delta.y());
    };
    std::stable_sort(order.begin(), order.end(),
                     [&](qsizetype a, qsizetype b) { return length(a) > length(b); });

    QList<QRectF> onTheLine;
    for (const qsizetype index : std::as_const(order))
    {
        const QPointF a = route.at(index - 1);
        const QPointF b = route.at(index);
        if (std::abs(a.x() - b.x()) < kSameCoordinate)
        {
            for (const qreal fraction : kLabelFractions)
            {
                const qreal centreY = a.y() + (b.y() - a.y()) * fraction;
                const qreal top = centreY - size.height() / 2;
                candidates.append(QRectF(QPointF(a.x() + kLabelOffset, top), size));
                candidates.append(QRectF(QPointF(a.x() - kLabelOffset - size.width(), top), size));
            }
            continue;
        }
        for (const qreal fraction : kLabelFractions)
        {
            const qreal centreX = a.x() + (b.x() - a.x()) * fraction;
            const qreal left = centreX - size.width() / 2;
            candidates.append(QRectF(QPointF(left, a.y() - size.height() - 2), size));
            candidates.append(QRectF(QPointF(left, a.y() + 2), size));
        }
        const qreal centreX = (a.x() + b.x()) / 2;
        onTheLine.append(QRectF(QPointF(centreX - size.width() / 2, a.y() - size.height() / 2), size));
    }
    candidates.append(onTheLine);
    return candidates;
}

namespace
{
    QRectF haloed(const QRectF& bounds, qreal halo) { return bounds.adjusted(-halo, 0, halo, 0); }

    int labelPenalty(const LabelSearch& search, const QRectF& bounds, qsizetype own,
                     const QList<QRectF>& taken)
    {
        const QRectF wide = haloed(bounds, search.halo);
        int penalty = 0;
        if (bounds.top() < 0 || bounds.bottom() > search.height)
            penalty += kHardPenalty;
        if (wide.left() < 0)
            penalty += kOverhangPenalty;
        for (const Obstacle& obstacle : *search.obstacles)
        {
            const QRectF box = obstacle.rect.adjusted(-kLabelHit, -kLabelHit, kLabelHit, kLabelHit);
            if (wide.intersects(box))
                penalty += kHardPenalty;
        }
        for (const QRectF& other : taken)
        {
            if (wide.intersects(haloed(other, search.halo)))
                penalty += kHardPenalty;
        }
        for (qsizetype index = 0; index < search.routes->size(); ++index)
        {
            if (routeTouches(wide, search.routes->at(index)))
                penalty += index == own ? kHardPenalty : kCrossedLinePenalty;
        }
        return penalty;
    }

    int placedLabel(const LabelSearch& search, qsizetype label, QList<QRectF>& bounds)
    {
        QList<QRectF> taken;
        for (const QRectF& other : std::as_const(bounds))
        {
            if (!other.isNull())
                taken.append(other);
        }
        const qsizetype own = search.ownRouteOf(label);
        int bestPenalty = std::numeric_limits<int>::max();
        for (const QRectF& candidate : search.candidatesOf(label))
        {
            const int penalty = labelPenalty(search, candidate, own, taken);
            if (penalty < bestPenalty)
            {
                bestPenalty = penalty;
                bounds[label] = candidate;
            }
        }
        return bestPenalty;
    }
}

bool placedLabels(const LabelSearch& search, const QList<qsizetype>& labels, QList<QRectF>& bounds)
{
    QHash<qsizetype, int> penalties;
    for (int pass = 0; pass <= kRefinePasses; ++pass)
    {
        if (pass > 0
            && std::none_of(penalties.cbegin(), penalties.cend(),
                            [](int penalty) { return penalty >= kHardPenalty; }))
            break;
        for (const qsizetype label : labels)
        {
            if (search.routes->at(search.ownRouteOf(label)).size() < 2)
                continue;
            bounds[label] = QRectF();
            penalties[label] = placedLabel(search, label, bounds);
        }
    }
    return std::none_of(penalties.cbegin(), penalties.cend(),
                        [](int penalty) { return penalty >= kHardPenalty; });
}

Scene narrowedUntilFits(const std::function<Scene(qreal contentWidth, bool& labelsClear)>& build,
                        qreal availableWidth, qreal margin, bool& labelsClear)
{
    const qreal contentWidth = std::max<qreal>(0, availableWidth - 2 * margin);
    Scene best = build(contentWidth, labelsClear);
    Scene latest = best;
    qreal limit = contentWidth;
    for (int attempt = 0; attempt < kMostNarrowerRetries; ++attempt)
    {
        const qreal overflow = latest.size.width() - availableWidth;
        if (overflow <= 0.5 || limit - overflow <= 0)
            break;
        limit -= overflow;
        bool narrowerClear = true;
        latest = build(limit, narrowerClear);
        if (latest.size.width() < best.size.width())
        {
            best = latest;
            labelsClear = narrowerClear;
        }
    }
    return best;
}

QList<int> longestPathLayers(const QList<QList<int>>& successors, const QList<int>& visitOrder)
{
    const qsizetype count = successors.size();
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
    for (const int node : visitOrder)
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
    return layerOf;
}

QList<QList<int>> groupedByLayer(const QList<int>& items, const QList<int>& layerOf)
{
    QList<QList<int>> layers;
    for (qsizetype position = 0; position < items.size(); ++position)
    {
        while (layers.size() <= layerOf[position])
            layers.append(QList<int> {});
        layers[layerOf[position]].append(items.at(position));
    }
    return layers;
}

LayeredFlow::LayeredFlow(LayeredFlowSource source, qreal gap)
    : m_source(std::move(source))
    , m_gap(gap)
{
}

QList<QList<int>> LayeredFlow::rowsOf(const QList<int>& layer, const QList<int>& siblings,
                                      qreal maxWidth) const
{
    const bool squeezes = std::any_of(layer.cbegin(), layer.cend(), [&](int item)
                                      { return m_source.linksAnother(item, siblings); });
    QList<QList<int>> rows;
    qreal rowWidth = 0;
    for (const int item : layer)
    {
        const qreal width = squeezes ? minimal(item).width : natural(item).width;
        if (!rows.isEmpty() && rowWidth + m_gap + width <= maxWidth + kFitTolerance)
        {
            rows.last().append(item);
            rowWidth += m_gap + width;
            continue;
        }
        rows.append(QList<int> { item });
        rowWidth = width;
    }
    return rows;
}

const Fragment& LayeredFlow::natural(int item) const
{
    auto found = m_natural.find(item);
    if (found == m_natural.end())
    {
        Fragment fragment = m_source.layoutItem(item, kUnbounded);
        found = m_natural.emplace(item, std::move(fragment)).first;
    }
    return found->second;
}

const Fragment& LayeredFlow::minimal(int item) const
{
    auto found = m_minimal.find(item);
    if (found == m_minimal.end())
    {
        Fragment fragment = m_source.layoutItem(item, 0);
        found = m_minimal.emplace(item, std::move(fragment)).first;
    }
    return found->second;
}

QList<qreal> LayeredFlow::limitsOf(const QList<int>& row, qreal maxWidth) const
{
    qreal naturalSum = 0;
    qreal minimalSum = 0;
    for (const int item : row)
    {
        naturalSum += natural(item).width;
        minimalSum += minimal(item).width;
    }
    const qreal budget = maxWidth - m_gap * qreal(row.size() - 1);
    const qreal spare = std::max<qreal>(0, budget - minimalSum);
    const qreal room = naturalSum - minimalSum;
    QList<qreal> limits;
    for (const int item : row)
    {
        const qreal naturalWidth = natural(item).width;
        if (naturalSum <= budget + kFitTolerance || room <= 0)
        {
            limits.append(naturalWidth);
            continue;
        }
        const qreal minimalWidth = minimal(item).width;
        limits.append(minimalWidth + spare * (naturalWidth - minimalWidth) / room);
    }
    return limits;
}

Fragment LayeredFlow::layoutRow(const QList<int>& row, qreal maxWidth, qreal reachBudget) const
{
    const QList<qreal> limits = limitsOf(row, maxWidth);
    QList<Fragment> items;
    for (qsizetype index = 0; index < row.size(); ++index)
    {
        const int item = row.at(index);
        const Fragment& whole = natural(item);
        if (limits.at(index) >= whole.width - kFitTolerance)
        {
            const bool overreaches = std::max(whole.reachLeft, whole.reachRight) > reachBudget;
            items.append(overreaches ? m_source.layoutItem(item, whole.width + 2 * reachBudget)
                                     : whole);
        }
        else if (limits.at(index) <= minimal(item).width + kFitTolerance)
            items.append(minimal(item));
        else
            items.append(m_source.layoutItem(item, limits.at(index)));
    }
    Fragment fragment;
    for (const Fragment& item : std::as_const(items))
        fragment.height = std::max(fragment.height, item.height);
    qreal x = 0;
    const Fragment* previous = nullptr;
    for (const Fragment& item : std::as_const(items))
    {
        if (previous)
            x += previous->reachRight + item.reachLeft;
        merge(fragment, item, x, centred(fragment.height - item.height));
        x += item.width + m_gap;
        previous = &item;
    }
    fragment.width = x - m_gap;
    if (!items.isEmpty())
    {
        fragment.reachLeft = items.first().reachLeft;
        fragment.reachRight = items.last().reachRight;
    }
    return fragment;
}

qreal LayeredFlow::fittingReach(const QList<Fragment>& rows, qsizetype index, qreal levelWidth,
                                qreal maxWidth) const
{
    qreal othersLeft = 0;
    qreal othersRight = 0;
    for (qsizetype other = 0; other < rows.size(); ++other)
    {
        if (other == index)
            continue;
        const Fragment& row = rows.at(other);
        const qreal offset = centred(levelWidth - row.width);
        othersLeft = std::max(othersLeft, row.reachLeft - offset);
        othersRight = std::max(othersRight, row.reachRight - (levelWidth - row.width - offset));
    }
    const Fragment& row = rows.at(index);
    const qreal offset = centred(levelWidth - row.width);
    const qreal slackRight = levelWidth - row.width - offset;
    const auto fits = [&](qreal reach)
    {
        const qreal left = std::max(othersLeft, std::min(row.reachLeft, reach) - offset);
        const qreal right = std::max(othersRight, std::min(row.reachRight, reach) - slackRight);
        return left + levelWidth + right <= maxWidth + kFitTolerance;
    };
    qreal low = 0;
    qreal high = std::max(row.reachLeft, row.reachRight);
    if (fits(high))
        return high;
    for (int step = 0; step < 24; ++step)
    {
        const qreal middle = (low + high) / 2;
        if (fits(middle))
            low = middle;
        else
            high = middle;
    }
    return low;
}

Fragment LayeredFlow::layoutLevel(const QList<int>& siblings, qreal maxWidth) const
{
    QList<QList<int>> rowItems;
    for (const QList<int>& layer : m_source.layersOf(siblings))
        rowItems.append(rowsOf(layer, siblings, maxWidth));
    QList<Fragment> rows;
    for (const QList<int>& row : std::as_const(rowItems))
        rows.append(layoutRow(row, maxWidth, kUnbounded));
    qreal levelWidth = 0;
    for (const Fragment& row : std::as_const(rows))
        levelWidth = std::max(levelWidth, row.width);
    if (maxWidth != kUnbounded)
    {
        for (qsizetype index = 0; index < rows.size(); ++index)
        {
            const Fragment& row = rows.at(index);
            if (std::max(row.reachLeft, row.reachRight) <= 0)
                continue;
            const qreal reach = fittingReach(rows, index, levelWidth, maxWidth);
            if (reach < std::max(row.reachLeft, row.reachRight))
                rows[index] = layoutRow(rowItems.at(index), maxWidth, reach);
        }
    }
    Fragment level;
    level.width = levelWidth;
    qreal y = 0;
    for (const Fragment& row : std::as_const(rows))
    {
        const qreal offset = centred(level.width - row.width);
        merge(level, row, offset, y);
        level.reachLeft = std::max(level.reachLeft, row.reachLeft - offset);
        level.reachRight
            = std::max(level.reachRight, row.reachRight - (level.width - row.width - offset));
        y += row.height + m_rowGap;
    }
    level.height = rows.isEmpty() ? 0 : y - m_rowGap;
    return level;
}
}
