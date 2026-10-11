#include <gtest/gtest.h>

#include "diagram/statelayout.h"
#include "diagram/stateparser.h"
#include "diagram/svgscenewriter.h"
#include "faketextmeasurer.h"

#include <algorithm>

using namespace diagram;

namespace
{
constexpr qreal kEpsilon = 0.01;

const QString kLifeCycle = QStringLiteral("[*] --> Ready\n"
                                          "Ready --> Running : scheduled\n"
                                          "Running --> Ready : preempted\n"
                                          "Running --> Blocked : waits for I/O\n"
                                          "Blocked --> Ready : I/O done\n"
                                          "Running --> [*] : exit\n");

const QString kComposite = QStringLiteral("state \"Device on\" as On {\n"
                                          "  [*] --> Idle\n"
                                          "  Idle --> Working : start\n"
                                          "  state Working {\n"
                                          "    [*] --> Heating\n"
                                          "    Heating --> Cooling : target reached\n"
                                          "    Cooling --> Heating : too cold\n"
                                          "  }\n"
                                          "  Working --> Idle : stop\n"
                                          "}\n"
                                          "[*] --> On\n"
                                          "On --> Off : power button\n"
                                          "Off --> On : power button\n"
                                          "note right of Off : standby draws little power\n");

const QString kRich = QStringLiteral("title Reader\n"
                                     "[*] --> Idle\n"
                                     "state Idle\n"
                                     "Idle : waits for a request\n"
                                     "Idle --> Reading : read request arrives\n"
                                     "Idle --> Writing : write request arrives\n"
                                     "Reading --> Idle : data returned to the host\n"
                                     "Writing --> Idle : data stored\n"
                                     "Writing --> Writing : retry\n"
                                     "Writing --> [*] : worn out\n"
                                     "note left of Reading : read only\n");

const QString kTcp = QStringLiteral("title TCP connection\n"
                                    "[*] --> Closed\n"
                                    "Closed --> Listen : listen()\n"
                                    "Listen --> Established : SYN, ACK\n"
                                    "state Established {\n"
                                    "  [*] --> Idle\n"
                                    "  Idle --> Sending : write()\n"
                                    "  Sending --> Idle : ACK\n"
                                    "}\n"
                                    "Established --> Closed : FIN\n"
                                    "Established : data flows\n"
                                    "note right of Listen : waits for SYN\n");

const QString kHandshake = QStringLiteral("title Connection life cycle\n"
                                          "[*] --> Closed\n"
                                          "Closed --> Listen : passive open\n"
                                          "Closed --> SynSent : active open\n"
                                          "Listen --> SynReceived : SYN received\n"
                                          "SynSent --> Established : SYN-ACK received\n"
                                          "SynReceived --> Established : ACK received\n"
                                          "Established --> Established : data\n"
                                          "Established --> Closed : close\n"
                                          "Closed --> [*]\n");

const QString kNestedBackEdges = QStringLiteral("[*] --> A\n"
                                                "A --> B : one\n"
                                                "B --> C : two\n"
                                                "C --> D : three\n"
                                                "D --> E : four\n"
                                                "E --> A : long way\n"
                                                "D --> B : middle\n"
                                                "C --> B : short\n");

const QList<qreal> kWidths { 240, 280, 320, 336, 360, 400, 480, 640 };

StateDiagram parsed(const QString& text)
{
    const auto result = StateParser::parse(text);
    EXPECT_TRUE(result.isSuccess()) << (result.isFailure() ? result.error().toStdString() : "");
    return result.isSuccess() ? result.value() : StateDiagram {};
}

Scene laidOut(const QString& text, qreal width)
{
    static const FakeTextMeasurer measurer;
    return StateLayout::layout(parsed(text), DiagramStyle {}, width, measurer);
}

template <typename T> QList<T> itemsOf(const Scene& scene)
{
    QList<T> found;
    for (const SceneItem& item : scene.items)
    {
        if (const auto* typed = std::get_if<T>(&item))
            found.append(*typed);
    }
    return found;
}

QRectF boundsOf(const QList<QPointF>& points)
{
    qreal left = points.first().x();
    qreal right = left;
    qreal top = points.first().y();
    qreal bottom = top;
    for (const QPointF& point : points)
    {
        left = std::min(left, point.x());
        right = std::max(right, point.x());
        top = std::min(top, point.y());
        bottom = std::max(bottom, point.y());
    }
    return QRectF(QPointF(left, top), QPointF(right, bottom));
}

bool inside(const QRectF& outer, const QRectF& inner)
{
    return inner.left() >= outer.left() - kEpsilon && inner.top() >= outer.top() - kEpsilon
        && inner.right() <= outer.right() + kEpsilon && inner.bottom() <= outer.bottom() + kEpsilon;
}

bool overlaps(const QRectF& a, const QRectF& b)
{
    const QRectF common = a.intersected(b);
    return common.width() > kEpsilon && common.height() > kEpsilon;
}

QList<QRectF> stateBoxes(const Scene& scene)
{
    QList<QRectF> boxes;
    for (const SceneRect& rect : itemsOf<SceneRect>(scene))
    {
        if (rect.fill == ColorRole::BoxFill && rect.stroke == ColorRole::BoxStroke)
            boxes.append(rect.rect);
    }
    return boxes;
}

QList<QRectF> noteBoxes(const Scene& scene)
{
    QList<QRectF> boxes;
    for (const SceneRect& rect : itemsOf<SceneRect>(scene))
    {
        if (rect.fill == ColorRole::NoteFill)
            boxes.append(rect.rect);
    }
    return boxes;
}

QList<QRectF> frameRects(const Scene& scene)
{
    QList<QRectF> frames;
    for (const SceneRect& rect : itemsOf<SceneRect>(scene))
    {
        if (!rect.fill && rect.stroke == ColorRole::GroupStroke)
            frames.append(rect.rect);
    }
    return frames;
}

QList<QRectF> labelHalos(const Scene& scene)
{
    QList<QRectF> halos;
    for (const SceneRect& rect : itemsOf<SceneRect>(scene))
    {
        if (rect.fill == ColorRole::Background && !rect.stroke)
            halos.append(rect.rect);
    }
    return halos;
}

QList<SceneLine> transitionLines(const Scene& scene)
{
    QList<SceneLine> lines;
    for (const SceneLine& line : itemsOf<SceneLine>(scene))
    {
        if (line.color == ColorRole::Line)
            lines.append(line);
    }
    return lines;
}

QRectF textBounds(const Scene& scene, const QString& shown)
{
    for (const SceneText& text : itemsOf<SceneText>(scene))
    {
        if (text.lines.join(QLatin1Char(' ')) == shown)
            return text.bounds;
    }
    ADD_FAILURE() << "no text " << shown.toStdString();
    return QRectF();
}

QList<QRectF> pseudoStateRects(const Scene& scene)
{
    QList<QRectF> rects;
    for (const SceneEllipse& ellipse : itemsOf<SceneEllipse>(scene))
    {
        if (ellipse.rect.width() > 15)
            rects.append(ellipse.rect);
    }
    return rects;
}

void expectInvariants(const Scene& scene, qreal availableWidth)
{
    const QRectF area(QPointF(0, 0), scene.size);
    for (const SceneRect& rect : itemsOf<SceneRect>(scene))
        EXPECT_TRUE(inside(area, rect.rect)) << availableWidth;
    for (const SceneEllipse& ellipse : itemsOf<SceneEllipse>(scene))
        EXPECT_TRUE(inside(area, ellipse.rect)) << availableWidth;
    for (const SceneLine& line : itemsOf<SceneLine>(scene))
    {
        for (const QPointF& point : line.points)
            EXPECT_TRUE(inside(area, QRectF(point, QSizeF(0, 0)))) << availableWidth;
    }
    for (const SceneText& text : itemsOf<SceneText>(scene))
        EXPECT_TRUE(inside(area, text.bounds))
            << availableWidth << " " << text.lines.join(QLatin1Char(' ')).toStdString();

    const QList<QRectF> boxes = stateBoxes(scene);
    for (qsizetype i = 0; i < boxes.size(); ++i)
    {
        for (qsizetype j = i + 1; j < boxes.size(); ++j)
            EXPECT_FALSE(overlaps(boxes[i], boxes[j])) << availableWidth;
    }

    const QList<QRectF> halos = labelHalos(scene);
    const QList<QRectF> notes = noteBoxes(scene);
    for (qsizetype i = 0; i < halos.size(); ++i)
    {
        for (const QRectF& box : boxes)
            EXPECT_FALSE(overlaps(halos[i], box)) << availableWidth;
        for (const QRectF& note : notes)
            EXPECT_FALSE(overlaps(halos[i], note)) << availableWidth;
        for (qsizetype j = i + 1; j < halos.size(); ++j)
            EXPECT_FALSE(overlaps(halos[i], halos[j])) << availableWidth;
    }
}

void expectNoTransitionThroughABox(const Scene& scene, qreal availableWidth)
{
    const QList<QRectF> boxes = stateBoxes(scene);
    for (const SceneLine& line : transitionLines(scene))
    {
        for (qsizetype index = 1; index < line.points.size(); ++index)
        {
            const QRectF segment = boundsOf({ line.points.at(index - 1), line.points.at(index) });
            for (const QRectF& box : boxes)
                EXPECT_FALSE(
                    overlaps(segment.adjusted(-0.4, -0.4, 0.4, 0.4), box.adjusted(1, 1, -1, -1)))
                    << availableWidth;
        }
    }
}

qreal rightmostBox(const Scene& scene)
{
    qreal right = 0;
    for (const QRectF& box : stateBoxes(scene))
        right = std::max(right, box.right());
    return right;
}

bool isBackEdge(const SceneLine& line)
{
    if (line.points.size() < 4)
        return false;
    const QPointF last = line.points.last();
    const QPointF before = line.points.at(line.points.size() - 2);
    return std::abs(last.y() - before.y()) < kEpsilon && last.x() < before.x() - kEpsilon
        && last.y() < line.points.first().y();
}

QList<SceneLine> backEdges(const Scene& scene)
{
    QList<SceneLine> edges;
    for (const SceneLine& line : transitionLines(scene))
    {
        if (isBackEdge(line))
            edges.append(line);
    }
    return edges;
}

bool segmentsCross(const QPointF& a1, const QPointF& a2, const QPointF& b1, const QPointF& b2)
{
    const auto isVertical
        = [](const QPointF& p, const QPointF& q) { return std::abs(p.x() - q.x()) < kEpsilon; };
    const auto isHorizontal
        = [](const QPointF& p, const QPointF& q) { return std::abs(p.y() - q.y()) < kEpsilon; };
    const auto strictlyBetween = [](qreal value, qreal p, qreal q)
    { return value > std::min(p, q) + kEpsilon && value < std::max(p, q) - kEpsilon; };
    if (isHorizontal(a1, a2) && isVertical(b1, b2))
        return strictlyBetween(b1.x(), a1.x(), a2.x()) && strictlyBetween(a1.y(), b1.y(), b2.y());
    if (isVertical(a1, a2) && isHorizontal(b1, b2))
        return segmentsCross(b1, b2, a1, a2);
    return false;
}

bool routesCross(const QList<QPointF>& first, const QList<QPointF>& second)
{
    for (qsizetype i = 1; i < first.size(); ++i)
    {
        for (qsizetype j = 1; j < second.size(); ++j)
        {
            if (segmentsCross(first.at(i - 1), first.at(i), second.at(j - 1), second.at(j)))
                return true;
        }
    }
    return false;
}

void expectNoBackEdgesCross(const Scene& scene, qreal availableWidth)
{
    const QList<SceneLine> edges = backEdges(scene);
    for (qsizetype i = 0; i < edges.size(); ++i)
    {
        for (qsizetype j = i + 1; j < edges.size(); ++j)
            EXPECT_FALSE(routesCross(edges.at(i).points, edges.at(j).points)) << availableWidth;
    }
}

qreal centreX(const Scene& scene, const QString& shown)
{
    return textBounds(scene, shown).center().x();
}
}

TEST(StateLayoutTest, TheLifeCycleExampleFitsAPhoneWidth)
{
    const Scene scene = laidOut(kLifeCycle, 336);

    EXPECT_LE(scene.size.width(), 336);
    expectInvariants(scene, 336);
    expectNoTransitionThroughABox(scene, 336);
    EXPECT_EQ(scene.fontPixelSize, DiagramStyle {}.fontPixelSize);
}

TEST(StateLayoutTest, KeepsTheInvariantsAtEveryWidth)
{
    for (const QString& text : { kLifeCycle, kComposite, kRich, kTcp, kHandshake })
    {
        for (const qreal width : kWidths)
        {
            const Scene scene = laidOut(text, width);
            expectInvariants(scene, width);
            expectNoTransitionThroughABox(scene, width);
        }
    }
}

TEST(StateLayoutTest, TheLifeCycleAndTheCompositeFitEveryPhoneWidth)
{
    for (const QString& text : { kLifeCycle, kComposite })
    {
        for (const qreal width : kWidths)
        {
            if (width >= 336)
                EXPECT_LE(laidOut(text, width).size.width(), width) << width;
        }
    }
}

TEST(StateLayoutTest, TheInitialStateIsOnTop)
{
    const Scene scene = laidOut(kLifeCycle, 336);

    const QList<QRectF> pseudo = pseudoStateRects(scene);
    ASSERT_EQ(pseudo.size(), 2);
    for (const QRectF& box : stateBoxes(scene))
        EXPECT_LT(pseudo.first().bottom(), box.top());
}

TEST(StateLayoutTest, TheFinalStateSitsRightAfterItsSourceBesideTheOtherTargets)
{
    for (const qreal width : { 336.0, 480.0 })
    {
        const Scene scene = laidOut(kLifeCycle, width);

        const QList<QRectF> pseudo = pseudoStateRects(scene);
        ASSERT_EQ(pseudo.size(), 2);
        const QRectF running = textBounds(scene, QStringLiteral("Running"));
        const QRectF blocked = textBounds(scene, QStringLiteral("Blocked"));
        const QRectF final = pseudo.last();
        EXPECT_GT(final.top(), running.bottom()) << width;
        EXPECT_GE(final.center().y(), blocked.top()) << width;
        EXPECT_LE(final.center().y(), blocked.bottom()) << width;
    }
}

TEST(StateLayoutTest, StatesFollowTheTransitionsDownTheLayers)
{
    const Scene scene = laidOut(kLifeCycle, 336);

    const QRectF ready = textBounds(scene, QStringLiteral("Ready"));
    const QRectF running = textBounds(scene, QStringLiteral("Running"));
    const QRectF blocked = textBounds(scene, QStringLiteral("Blocked"));
    EXPECT_LT(ready.bottom(), running.top());
    EXPECT_LT(running.bottom(), blocked.top());
}

TEST(StateLayoutTest, EveryTransitionHasAnArrowHeadAtItsTarget)
{
    const Scene scene = laidOut(kLifeCycle, 336);

    const QList<SceneLine> lines = transitionLines(scene);
    ASSERT_EQ(lines.size(), 6);
    for (const SceneLine& line : lines)
        EXPECT_EQ(line.head, ArrowHead::Filled);
}

TEST(StateLayoutTest, ABackEdgeRunsOutsideTheBoxes)
{
    const Scene scene = laidOut(kLifeCycle, 336);

    const qreal boxesRight = rightmostBox(scene);
    const QList<SceneLine> edges = backEdges(scene);
    EXPECT_EQ(edges.size(), 2);
    for (const SceneLine& line : edges)
        EXPECT_GT(boundsOf(line.points).right(), boxesRight);
}

TEST(StateLayoutTest, BackEdgesNestWithoutCrossingAndEnterTheTargetAtOwnHeights)
{
    for (const QString& text :
         { kLifeCycle, kComposite, kRich, kTcp, kHandshake, kNestedBackEdges })
    {
        SCOPED_TRACE(text.toStdString());
        for (const qreal width : kWidths)
            expectNoBackEdgesCross(laidOut(text, width), width);
    }

    const QList<SceneLine> edges = backEdges(laidOut(kNestedBackEdges, 480));
    ASSERT_EQ(edges.size(), 3);
    QList<qreal> entryHeights;
    qreal outermost = 0;
    qreal longest = 0;
    for (const SceneLine& line : edges)
    {
        entryHeights.append(line.points.last().y());
        const qreal span = line.points.first().y() - line.points.last().y();
        const qreal laneX = boundsOf(line.points).right();
        if (span > longest)
        {
            longest = span;
            outermost = laneX;
        }
    }
    for (const SceneLine& line : edges)
        EXPECT_LE(boundsOf(line.points).right(), outermost + kEpsilon);
    std::sort(entryHeights.begin(), entryHeights.end());
    EXPECT_GT(entryHeights.at(1) - entryHeights.at(0), 1);
    EXPECT_GT(entryHeights.at(2) - entryHeights.at(1), 1);
}

TEST(StateLayoutTest, NoLabelTouchesAnyLineItsOwnEdgeIncluded)
{
    for (const QString& text : { kLifeCycle, kTcp, kNestedBackEdges })
    {
        for (const qreal width : kWidths)
        {
            if (width < 280)
                continue;
            const Scene scene = laidOut(text, width);
            for (const QRectF& halo : labelHalos(scene))
            {
                for (const SceneLine& line : transitionLines(scene))
                {
                    for (qsizetype index = 1; index < line.points.size(); ++index)
                    {
                        const QRectF segment
                            = boundsOf({ line.points.at(index - 1), line.points.at(index) })
                                  .adjusted(-0.5, -0.5, 0.5, 0.5);
                        EXPECT_FALSE(overlaps(halo, segment)) << width;
                    }
                }
            }
        }
    }
}

TEST(StateLayoutTest, ABackEdgeLabelSitsBesideItsVerticalOnTheOuterSide)
{
    const Scene scene = laidOut(kLifeCycle, 336);

    qreal outermostLine = 0;
    for (const SceneLine& line : backEdges(scene))
        outermostLine = std::max(outermostLine, boundsOf(line.points).right());
    EXPECT_GT(textBounds(scene, QStringLiteral("I/O done")).left(), outermostLine);
}

TEST(StateLayoutTest, ATransitionBetweenStatesInsideACompositeStaysInsideItsFrame)
{
    for (const QString& text : { kComposite, kTcp })
    {
        for (const qreal width : kWidths)
        {
            const Scene scene = laidOut(text, width);
            const QList<QRectF> frames = frameRects(scene);
            qsizetype checked = 0;
            for (const SceneLine& line : backEdges(scene))
            {
                for (const QRectF& box : stateBoxes(scene))
                {
                    if (std::abs(line.points.first().x() - box.right()) > kEpsilon
                        || line.points.first().y() < box.top()
                        || line.points.first().y() > box.bottom())
                        continue;
                    QRectF smallest;
                    for (const QRectF& frame : frames)
                    {
                        if (inside(frame, box) && (smallest.isNull() || inside(smallest, frame)))
                            smallest = frame;
                    }
                    if (smallest.isNull())
                        continue;
                    EXPECT_TRUE(inside(smallest, boundsOf(line.points))) << width;
                    ++checked;
                }
            }
            EXPECT_GE(checked, 1) << width;
        }
    }
}

TEST(StateLayoutTest, ATransitionLeavingACompositeStartsFromItsFrameAtHeaderHeight)
{
    for (const qreal width : kWidths)
    {
        const Scene scene = laidOut(kTcp, width);
        const QList<QRectF> frames = frameRects(scene);
        ASSERT_EQ(frames.size(), 1);
        const QRectF frame = frames.first();
        const qreal headerBottom = textBounds(scene, QStringLiteral("data flows")).bottom();

        qsizetype leaving = 0;
        for (const SceneLine& line : transitionLines(scene))
        {
            if (std::abs(line.points.first().x() - frame.right()) > kEpsilon)
                continue;
            EXPECT_GE(line.points.first().y(), frame.top()) << width;
            EXPECT_LE(line.points.first().y(), headerBottom) << width;
            ++leaving;
        }
        EXPECT_EQ(leaving, 1) << width;
    }
}

TEST(StateLayoutTest, ANoteDoesNotShiftItsStateOffTheColumn)
{
    const QString chain = QStringLiteral("A --> Mid\nMid --> C\n");
    for (const QString& note : { QStringLiteral("note right of Mid : hangs on the right\n"),
                                 QStringLiteral("note left of Mid : hangs on the left\n"),
                                 QStringLiteral("note left of Mid : on the left\n"
                                                "note right of Mid : on the right\n") })
    {
        for (const qreal width : { 336.0, 400.0, 480.0, 640.0 })
        {
            const Scene scene = laidOut(chain + note, width);
            const qreal column = centreX(scene, QStringLiteral("Mid"));
            EXPECT_NEAR(centreX(scene, QStringLiteral("A")), column, 1) << width;
            EXPECT_NEAR(centreX(scene, QStringLiteral("C")), column, 1) << width;
            expectInvariants(scene, width);
            expectNoTransitionThroughABox(scene, width);
        }
    }

    for (const qreal width : kWidths)
    {
        const Scene scene = laidOut(kTcp, width);
        EXPECT_NEAR(centreX(scene, QStringLiteral("Closed")),
                    centreX(scene, QStringLiteral("Listen")), 1)
            << width;
        EXPECT_GT(textBounds(scene, QStringLiteral("waits for SYN")).left(),
                  textBounds(scene, QStringLiteral("Listen")).right())
            << width;
    }
}

TEST(StateLayoutTest, ASelfTransitionIsALoopBesideItsState)
{
    const Scene scene = laidOut(kRich, 360);

    const QRectF writing = textBounds(scene, QStringLiteral("Writing"));
    qsizetype loops = 0;
    for (const SceneLine& line : transitionLines(scene))
    {
        if (line.points.size() != 4 || line.points.first().x() != line.points.last().x())
            continue;
        EXPECT_LT(line.points.at(1).x(), line.points.first().x());
        EXPECT_NEAR(line.points.at(1).y(), line.points.at(0).y(), kEpsilon);
        EXPECT_NEAR(line.points.at(2).y(), line.points.at(3).y(), kEpsilon);
        if (line.points.first().y() >= writing.top() - 20
            && line.points.last().y() <= writing.bottom() + 20)
            ++loops;
    }
    EXPECT_EQ(loops, 1);
    EXPECT_LT(textBounds(scene, QStringLiteral("retry")).right(), writing.left());
    expectInvariants(scene, 360);
}

TEST(StateLayoutTest, ACompositeStateFramesItsChildrenWithItsNameInAHeader)
{
    const Scene scene = laidOut(kComposite, 336);

    const QList<QRectF> frames = frameRects(scene);
    ASSERT_EQ(frames.size(), 2);
    const QRectF outer = frames.at(0).width() > frames.at(1).width() ? frames.at(0) : frames.at(1);
    const QRectF inner = frames.at(0).width() > frames.at(1).width() ? frames.at(1) : frames.at(0);
    EXPECT_TRUE(inside(outer, inner));
    for (const QString& child : { QStringLiteral("Idle"), QStringLiteral("Working") })
        EXPECT_TRUE(inside(outer, textBounds(scene, child))) << child.toStdString();
    for (const QString& child : { QStringLiteral("Heating"), QStringLiteral("Cooling") })
    {
        EXPECT_TRUE(inside(inner, textBounds(scene, child))) << child.toStdString();
        EXPECT_TRUE(inside(outer, textBounds(scene, child))) << child.toStdString();
    }
    EXPECT_TRUE(inside(outer, textBounds(scene, QStringLiteral("Device on"))));
    EXPECT_LT(textBounds(scene, QStringLiteral("Device on")).bottom(),
              textBounds(scene, QStringLiteral("Idle")).top());
    EXPECT_FALSE(overlaps(outer, textBounds(scene, QStringLiteral("Off"))));
    EXPECT_GT(textBounds(scene, QStringLiteral("Off")).top(), outer.bottom());
}

TEST(StateLayoutTest, ADescriptionIsDrawnUnderTheNameOfItsState)
{
    const Scene scene = laidOut(QStringLiteral("state Idle\nIdle : waits for input\n"
                                               "Idle --> Busy\n"),
                                336);

    EXPECT_LT(textBounds(scene, QStringLiteral("Idle")).bottom(),
              textBounds(scene, QStringLiteral("waits for input")).top());
    EXPECT_EQ(itemsOf<SceneLine>(scene).size(), 2);
    expectInvariants(scene, 336);
}

TEST(StateLayoutTest, AStateWithoutTransitionsStillGetsABox)
{
    const Scene scene = laidOut(QStringLiteral("state Alone\n"), 336);

    ASSERT_EQ(stateBoxes(scene).size(), 1);
    expectInvariants(scene, 336);
}

TEST(StateLayoutTest, ParallelAndOppositeTransitionsDoNotShareALine)
{
    const Scene scene = laidOut(QStringLiteral("A --> B : first\n"
                                               "A --> B : second\n"
                                               "B --> A : back\n"),
                                336);

    const QList<SceneLine> lines = transitionLines(scene);
    ASSERT_EQ(lines.size(), 3);
    for (qsizetype i = 0; i < lines.size(); ++i)
    {
        for (qsizetype j = i + 1; j < lines.size(); ++j)
            EXPECT_NE(lines.at(i).points, lines.at(j).points);
    }
    expectInvariants(scene, 336);
    expectNoTransitionThroughABox(scene, 336);
}

TEST(StateLayoutTest, ALabelSitsOnAHaloAndTouchesNoBox)
{
    const Scene scene = laidOut(QStringLiteral("A --> B : goes\n"), 336);

    const QList<QRectF> halos = labelHalos(scene);
    ASSERT_EQ(halos.size(), 1);
    const QRectF label = textBounds(scene, QStringLiteral("goes"));
    EXPECT_TRUE(inside(halos.first(), label));
    for (const QRectF& box : stateBoxes(scene))
        EXPECT_FALSE(overlaps(halos.first(), box));
}

TEST(StateLayoutTest, NotesSitBesideTheirStateOnTheRequestedSide)
{
    const Scene scene = laidOut(QStringLiteral("A --> B\n"
                                               "note left of A : on the left\n"
                                               "note right of B : on the right\n"),
                                480);

    EXPECT_LT(textBounds(scene, QStringLiteral("on the left")).right(),
              textBounds(scene, QStringLiteral("A")).left());
    EXPECT_GT(textBounds(scene, QStringLiteral("on the right")).left(),
              textBounds(scene, QStringLiteral("B")).right());
    EXPECT_EQ(noteBoxes(scene).size(), 2);
    expectInvariants(scene, 480);
}

TEST(StateLayoutTest, TheTitleSitsAboveTheDiagram)
{
    const Scene scene = laidOut(QStringLiteral("title The machine\nA --> B\n"), 336);

    EXPECT_LT(textBounds(scene, QStringLiteral("The machine")).bottom(),
              textBounds(scene, QStringLiteral("A")).top());
}

TEST(StateLayoutTest, ANarrowerWidthWrapsTextAndKeepsTheFontSize)
{
    const QString text = QStringLiteral("state \"A state whose name is long enough to need several "
                                        "lines on a phone\" as B\n"
                                        "A --> B\n");
    const auto lineCount = [](const Scene& scene)
    {
        qsizetype count = 0;
        for (const SceneText& item : itemsOf<SceneText>(scene))
            count += item.lines.size();
        return count;
    };

    const Scene wide = laidOut(text, 640);
    const Scene narrow = laidOut(text, 240);

    EXPECT_GT(lineCount(narrow), lineCount(wide));
    EXPECT_EQ(narrow.fontPixelSize, wide.fontPixelSize);
}

TEST(StateLayoutTest, DrawsOnlyWhatTheWriterCanWrite)
{
    const QString svg = SvgSceneWriter::write(laidOut(kComposite, 336), DiagramPalette {}, 1);

    EXPECT_TRUE(svg.contains(QStringLiteral("<ellipse")));
    EXPECT_TRUE(svg.contains(QStringLiteral("<polygon")));
    EXPECT_TRUE(svg.contains(QStringLiteral("<polyline")));
}

TEST(StateLayoutTest, SamePieceOfTextAlwaysGivesTheSameScene)
{
    const Scene first = laidOut(kComposite, 336);
    const Scene second = laidOut(kComposite, 336);

    EXPECT_EQ(first.size, second.size);
    EXPECT_EQ(first.items.size(), second.items.size());
}
