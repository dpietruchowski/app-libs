#pragma once

#include "diagram/diagramstyle.h"
#include "diagram/scene.h"
#include "diagram/textmeasurer.h"

#include <QList>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>

#include <functional>
#include <limits>
#include <map>
#include <utility>

namespace diagram::layouting
{
struct WrappedText
{
    QStringList lines;
    qreal width = 0;
    qreal height = 0;
};

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
    qreal reachLeft = 0;
    qreal reachRight = 0;
    QList<SceneItem> items;
    std::map<int, QRectF> rects;
    QList<Obstacle> obstacles;
};

qreal wholePixels(qreal value);
qreal centred(qreal space);

QList<qreal> fairWidths(const QList<qreal>& minimal, qreal usable);

class TextWrapper
{
public:
    TextWrapper(const DiagramStyle& style, const TextMeasurer& measurer);

    qreal measured(const QString& text) const;
    qreal widestWord(const QString& text) const;
    qreal minimumWrapWidth() const;
    WrappedText wrap(const QString& text, qreal maxWidth) const;
    WrappedText wrapKeepingWords(const QString& text, qreal maxWidth) const;
    qreal heightOf(const WrappedText& text) const;

private:
    void wrapParagraph(const QString& paragraph, qreal limit, QStringList& lines) const;
    QString breakLongWord(const QString& word, qreal limit, QStringList& lines) const;

    const DiagramStyle& m_style;
    const TextMeasurer& m_measurer;
};

void appendShifted(QList<SceneItem>& target, const QList<SceneItem>& source, qreal dx, qreal dy);

void merge(Fragment& target, const Fragment& part, qreal dx, qreal dy);

QRectF placeFrameTab(QList<SceneItem>& items, const WrappedText& label, const DiagramStyle& style,
                     const QPointF& topLeft);

bool isFree(const Route& route, const QList<Obstacle>& obstacles, int fromBlock, int toBlock);

QRectF keptInside(QRectF bounds, const QSizeF& size, qreal halo);

void appendHaloLabel(QList<SceneItem>& items, const QRectF& bounds, const WrappedText& label,
                     qreal halo);

struct NoteBox
{
    WrappedText text;
    qreal width = 0;
    qreal height = 0;
};

NoteBox noteBoxOf(const QString& text, qreal limit, const TextWrapper& wrapper,
                  const DiagramStyle& style);

void placeNote(Fragment& fragment, const NoteBox& box, qreal x, qreal y, const DiagramStyle& style);

inline constexpr qreal kSameCoordinate = 0.5;
inline constexpr qreal kLaneClearance = 8;
inline constexpr qreal kLaneSpacing = 8;
inline constexpr qreal kLabelOffset = 6;

enum class Side
{
    Top,
    Bottom,
    Left,
    Right
};

Route withoutRepeats(const Route& route);

Route downRouteVia(const QPointF& start, const QPointF& end, qreal laneOffset = 0,
                   qreal laneFloor = -std::numeric_limits<qreal>::infinity(),
                   qreal laneCeiling = std::numeric_limits<qreal>::infinity());

std::pair<qreal, qreal> alignedX(const QRectF& from, const QRectF& to, qreal exitX, qreal entryX,
                                 bool exitAlone, bool entryAlone);

QPointF portAt(const QRectF& rect, Side side, qreal fraction);

qreal portKeyFor(Side side, const QRectF& other);

struct PortEnd
{
    int edge = -1;
    bool isExit = false;
    int box = -1;
    Side side = Side::Bottom;
    QRectF other;
};

struct PortPlacement
{
    QPointF point;
    bool alone = true;
};

using PortPlacements = std::map<std::pair<int, bool>, PortPlacement>;

PortPlacements allocatedPorts(const QList<PortEnd>& ends,
                              const std::function<QRectF(int box, Side side)>& rectOf,
                              const std::function<qreal(int box)>& centreYOf);

struct DownEnds
{
    int owner = -1;
    QPointF start;
    QPointF end;
    qreal laneFloor = -std::numeric_limits<qreal>::infinity();
    qreal laneCeiling = std::numeric_limits<qreal>::infinity();
};

QList<Route> routedDown(const QList<DownEnds>& ends);

int crossingCount(const Route& first, const Route& second);

bool routeTouches(const QRectF& bounds, const Route& route);

struct LaneRequest
{
    qreal low = 0;
    qreal high = 0;
    qreal labelWidth = 0;
    qreal minimumX = 0;
    std::function<Route(qreal laneX)> route;
};

QList<Route> routedInLanes(const QList<LaneRequest>& requests,
                           const std::function<qreal(qreal low, qreal high)>& rightEdgeWithin);

QList<QRectF> labelCandidatesAlong(const Route& route, const QSizeF& size, bool besideOuterVertical);

struct LabelSearch
{
    std::function<QList<QRectF>(qsizetype label)> candidatesOf;
    std::function<qsizetype(qsizetype label)> ownRouteOf;
    const QList<Route>* routes = nullptr;
    const QList<Obstacle>* obstacles = nullptr;
    qreal height = 0;
    qreal halo = 0;
};

bool placedLabels(const LabelSearch& search, const QList<qsizetype>& labels,
                  QList<QRectF>& bounds);

Scene narrowedUntilFits(const std::function<Scene(qreal contentWidth, bool& labelsClear)>& build,
                        qreal availableWidth, qreal margin, bool& labelsClear);

QList<int> longestPathLayers(const QList<QList<int>>& successors, const QList<int>& visitOrder);

QList<QList<int>> groupedByLayer(const QList<int>& items, const QList<int>& layerOf);

struct LayeredFlowSource
{
    std::function<Fragment(int item, qreal maxWidth)> layoutItem;
    std::function<QList<QList<int>>(const QList<int>& siblings)> layersOf;
    std::function<bool(int item, const QList<int>& siblings)> linksAnother;
};

class LayeredFlow
{
public:
    LayeredFlow(LayeredFlowSource source, qreal gap);

    void setRowGap(qreal rowGap) { m_rowGap = rowGap; }
    Fragment layoutLevel(const QList<int>& siblings, qreal maxWidth) const;

private:
    QList<QList<int>> rowsOf(const QList<int>& layer, const QList<int>& siblings,
                             qreal maxWidth) const;
    const Fragment& natural(int item) const;
    const Fragment& minimal(int item) const;
    QList<qreal> limitsOf(const QList<int>& row, qreal maxWidth) const;
    Fragment layoutRow(const QList<int>& row, qreal maxWidth, qreal reachBudget) const;
    qreal fittingReach(const QList<Fragment>& rows, qsizetype index, qreal levelWidth,
                       qreal maxWidth) const;

    LayeredFlowSource m_source;
    qreal m_gap;
    qreal m_rowGap = 0;
    mutable std::map<int, Fragment> m_natural;
    mutable std::map<int, Fragment> m_minimal;
};
}
