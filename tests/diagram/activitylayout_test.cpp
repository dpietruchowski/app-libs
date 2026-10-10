#include <QLineF>
#include <gtest/gtest.h>

#include "diagram/activitylayout.h"
#include "diagram/activityparser.h"
#include "diagram/svgscenewriter.h"
#include "faketextmeasurer.h"

#include <algorithm>
#include <cmath>

using namespace diagram;

namespace
{
constexpr qreal kEpsilon = 0.01;

const QString kRichDiagram
    = QStringLiteral("title Handling an order from the web shop to the customer\n"
                     "start\n"
                     ":Receive the order from the customer;\n"
                     "note right : Orders arrive through the web shop and the phone line\n"
                     "-> validated;\n"
                     "partition Checkout {\n"
                     "  if (Is the payment valid?) then (yes)\n"
                     "    :Charge the card;\n"
                     "  elseif (Is there a voucher?) then (maybe)\n"
                     "    :Apply the voucher;\n"
                     "    note left : Vouchers expire after thirty days\n"
                     "  else (no)\n"
                     "    :Reject the order;\n"
                     "    detach\n"
                     "  endif\n"
                     "  while (More items in the basket?) is (yes)\n"
                     "    :Pack the next item;\n"
                     "  endwhile (no)\n"
                     "  partition \"Inner partition\" {\n"
                     "    :Weigh the parcel;\n"
                     "  }\n"
                     "}\n"
                     "fork\n"
                     "  :Send the invoice;\n"
                     "fork again\n"
                     "  :Notify the warehouse;\n"
                     "  note left : urgent\n"
                     "fork again\n"
                     "  -> late;\n"
                     "  :Update the statistics;\n"
                     "end fork\n"
                     "repeat :Try to deliver;\n"
                     "  :Leave a notice;\n"
                     "repeat while (Delivered?) is (no) not (yes)\n"
                     ":Close the order;\n"
                     "if (Was it a gift?) then (yes)\n"
                     "  :Add a card;\n"
                     "endif\n"
                     "stop\n");

const QList<qreal> kWidths { 120, 160, 200, 240, 280, 320, 360, 400, 480, 560, 640, 720 };

ActivityDiagram parsed(const QString& text)
{
    const auto result = ActivityParser::parse(text);
    EXPECT_TRUE(result.isSuccess()) << (result.isFailure() ? result.error().toStdString() : "");
    return result.isSuccess() ? result.value() : ActivityDiagram {};
}

Scene laidOut(const QString& text, qreal width)
{
    static const FakeTextMeasurer measurer;
    return ActivityLayout::layout(parsed(text), DiagramStyle {}, width, measurer);
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

QList<QRectF> nodeRects(const Scene& scene)
{
    QList<QRectF> nodes;
    for (const SceneRect& rect : itemsOf<SceneRect>(scene))
    {
        if (rect.fill)
            nodes.append(rect.rect);
    }
    for (const SceneEllipse& ellipse : itemsOf<SceneEllipse>(scene))
        nodes.append(ellipse.rect);
    for (const ScenePolygon& polygon : itemsOf<ScenePolygon>(scene))
        nodes.append(boundsOf(polygon.points));
    return nodes;
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

bool endsOnTopEdgeOrSideVertex(const Scene& scene, const QPointF& end)
{
    for (const SceneRect& rect : itemsOf<SceneRect>(scene))
    {
        if (rect.fill && std::abs(rect.rect.top() - end.y()) < kEpsilon
            && end.x() >= rect.rect.left() - kEpsilon && end.x() <= rect.rect.right() + kEpsilon)
            return true;
    }
    for (const SceneEllipse& ellipse : itemsOf<SceneEllipse>(scene))
    {
        if (std::abs(ellipse.rect.top() - end.y()) < kEpsilon
            && end.x() >= ellipse.rect.left() - kEpsilon
            && end.x() <= ellipse.rect.right() + kEpsilon)
            return true;
    }
    for (const ScenePolygon& polygon : itemsOf<ScenePolygon>(scene))
    {
        const QRectF bounds = boundsOf(polygon.points);
        if (std::abs(bounds.top() - end.y()) < kEpsilon && end.x() >= bounds.left() - kEpsilon
            && end.x() <= bounds.right() + kEpsilon)
            return true;
        for (const QPointF& vertex : polygon.points)
        {
            if (QLineF(vertex, end).length() < kEpsilon)
                return true;
        }
    }
    return false;
}

qsizetype textLineCount(const Scene& scene)
{
    qsizetype count = 0;
    for (const SceneText& text : itemsOf<SceneText>(scene))
        count += text.lines.size();
    return count;
}

void expectInvariants(const Scene& scene, qreal availableWidth)
{
    const QRectF area(QPointF(0, 0), scene.size);
    for (const SceneRect& rect : itemsOf<SceneRect>(scene))
        EXPECT_TRUE(inside(area, rect.rect)) << availableWidth;
    for (const SceneEllipse& ellipse : itemsOf<SceneEllipse>(scene))
        EXPECT_TRUE(inside(area, ellipse.rect)) << availableWidth;
    for (const ScenePolygon& polygon : itemsOf<ScenePolygon>(scene))
        EXPECT_TRUE(inside(area, boundsOf(polygon.points))) << availableWidth;
    for (const SceneLine& line : itemsOf<SceneLine>(scene))
    {
        for (const QPointF& point : line.points)
            EXPECT_TRUE(inside(area, QRectF(point, QSizeF(0, 0)))) << availableWidth;
    }

    const QList<QRectF> nodes = nodeRects(scene);
    const QList<SceneText> texts = itemsOf<SceneText>(scene);
    for (qsizetype i = 0; i < texts.size(); ++i)
    {
        const std::string shown = texts[i].lines.join(QLatin1Char(' ')).toStdString();
        EXPECT_TRUE(inside(area, texts[i].bounds)) << availableWidth << " " << shown;
        for (const QString& line : texts[i].lines)
            EXPECT_LE(FakeTextMeasurer().width(line), texts[i].bounds.width() + kEpsilon);
        for (qsizetype j = i + 1; j < texts.size(); ++j)
        {
            EXPECT_FALSE(overlaps(texts[i].bounds, texts[j].bounds))
                << availableWidth << " texts overlap: " << shown << " / "
                << texts[j].lines.join(QLatin1Char(' ')).toStdString();
        }
        for (const QRectF& node : nodes)
        {
            if (overlaps(texts[i].bounds, node))
                EXPECT_TRUE(inside(node, texts[i].bounds))
                    << availableWidth << " text crosses a node: " << shown;
        }
    }

    for (const SceneLine& line : itemsOf<SceneLine>(scene))
    {
        if (line.head == ArrowHead::None)
            continue;
        EXPECT_TRUE(endsOnTopEdgeOrSideVertex(scene, line.points.last()))
            << availableWidth << " connector ends at " << line.points.last().x() << ","
            << line.points.last().y();
    }

    const QList<QRectF> frames = frameRects(scene);
    for (qsizetype i = 0; i < frames.size(); ++i)
    {
        for (qsizetype j = i + 1; j < frames.size(); ++j)
        {
            const bool nested = inside(frames[i], frames[j]) || inside(frames[j], frames[i]);
            EXPECT_TRUE(nested || !overlaps(frames[i], frames[j])) << availableWidth;
        }
    }
}

const SceneEllipse& firstEllipse(const QList<SceneEllipse>& ellipses) { return ellipses.first(); }

QRectF nodeAround(const Scene& scene, const QString& text)
{
    for (const SceneText& shown : itemsOf<SceneText>(scene))
    {
        if (shown.lines.join(QLatin1Char(' ')) != text)
            continue;
        for (const QRectF& node : nodeRects(scene))
        {
            if (inside(node, shown.bounds))
                return node;
        }
    }
    ADD_FAILURE() << "no node around " << text.toStdString();
    return QRectF();
}

const QString kNestedIf = QStringLiteral(
    "start\n"
    ":The MMU raises a page fault because the virtual address has no valid entry in the page "
    "table;\n"
    "if (Address inside a valid VMA?) then (yes)\n"
    "  if (Access allowed by the VMA flags?) then (yes)\n"
    "    :Allocate a physical page and map it;\n"
    "  else (no)\n"
    "    :Send SIGSEGV;\n"
    "    kill\n"
    "  endif\n"
    "else (no)\n"
    "  :Send SIGSEGV to the process;\n"
    "  kill\n"
    "endif\n"
    ":Return to the faulting instruction;\n"
    "stop\n");

const QString kForkWithNote = QStringLiteral("start\n"
                                             ":Top half acknowledges the device;\n"
                                             "partition \"Bottom half\" {\n"
                                             "  fork\n"
                                             "    :Copy the data to the buffer;\n"
                                             "    note left : runs with interrupts enabled\n"
                                             "  fork again\n"
                                             "    :Wake the waiting reader;\n"
                                             "  end fork\n"
                                             "}\n"
                                             "stop\n");

qsizetype lineCountOf(const Scene& scene, const QString& text)
{
    for (const SceneText& shown : itemsOf<SceneText>(scene))
    {
        if (shown.lines.join(QLatin1Char(' ')) == text)
            return shown.lines.size();
    }
    ADD_FAILURE() << "no text " << text.toStdString();
    return 0;
}

QList<SceneRect> forkBars(const Scene& scene)
{
    QList<SceneRect> bars;
    for (const SceneRect& rect : itemsOf<SceneRect>(scene))
    {
        if (rect.fill == ColorRole::Line)
            bars.append(rect);
    }
    return bars;
}
}

TEST(ActivityLayoutTest, KeepsEverythingConsistentAtEveryWidth)
{
    for (const qreal width : kWidths)
        expectInvariants(laidOut(kRichDiagram, width), width);
}

TEST(ActivityLayoutTest, NeverExceedsTheWidthExceptWhenTheMinimumDoesNot)
{
    const qreal minimum = laidOut(kRichDiagram, 0).size.width();

    for (const qreal width : kWidths)
    {
        const Scene scene = laidOut(kRichDiagram, width);
        EXPECT_LE(scene.size.width(), std::max(width, minimum) + kEpsilon) << width;
    }
    EXPECT_LE(laidOut(kRichDiagram, 720).size.width(), 720);
}

TEST(ActivityLayoutTest, OverflowLaysOutAtTheMinimumAndKeepsTheInvariants)
{
    const Scene tiny = laidOut(kRichDiagram, 20);

    EXPECT_NEAR(tiny.size.width(), laidOut(kRichDiagram, 0).size.width(), kEpsilon);
    expectInvariants(tiny, 20);
}

TEST(ActivityLayoutTest, ANarrowerWidthGivesMoreLinesAndNeverSmallerText)
{
    qsizetype previousLines = 0;
    for (auto width = kWidths.crbegin(); width != kWidths.crend(); ++width)
    {
        const Scene scene = laidOut(kRichDiagram, *width);
        EXPECT_EQ(scene.fontPixelSize, DiagramStyle {}.fontPixelSize);
        EXPECT_GE(textLineCount(scene), previousLines) << *width;
        previousLines = textLineCount(scene);
    }
}

TEST(ActivityLayoutTest, WidensWithTheWidthUpToTheNaturalSize)
{
    const qreal natural = laidOut(kRichDiagram, 100000).size.width();

    EXPECT_NEAR(laidOut(kRichDiagram, 100000).size.width(),
                laidOut(kRichDiagram, 90000).size.width(), kEpsilon);
    EXPECT_GT(natural, 360);
    EXPECT_LE(laidOut(kRichDiagram, 480).size.width(), natural);
}

TEST(ActivityLayoutTest, LongUnbreakableWordsAreBrokenNotOverflowed)
{
    const QString text = QStringLiteral("title Supercalifragilisticexpialidocious_title_word\n"
                                        "start\n"
                                        ":Pneumonoultramicroscopicsilicovolcanoconiosis_and_more;\n"
                                        "if (Floccinaucinihilipilificationisticallyhuge?) then "
                                        "(Antidisestablishmentarianism)\n"
                                        ":x;\n"
                                        "endif\n"
                                        "stop\n");

    for (const qreal width : { 120.0, 240.0, 360.0 })
        expectInvariants(laidOut(text, width), width);
}

TEST(ActivityLayoutTest, NestedStructuresKeepTheirInvariants)
{
    const QString text = QStringLiteral("start\n"
                                        "partition A {\n"
                                        "partition B {\n"
                                        "partition C {\n"
                                        "if (one?) then (y)\n"
                                        "if (two?) then (y)\n"
                                        "while (three?) is (y)\n"
                                        "fork\n"
                                        ":left branch with a longer text;\n"
                                        "note right : side note for the left branch\n"
                                        "fork again\n"
                                        "repeat\n"
                                        ":right;\n"
                                        "repeat while (again?) is (y) not (n)\n"
                                        "end fork\n"
                                        "endwhile (n)\n"
                                        "else (n)\n"
                                        ":other;\n"
                                        "endif\n"
                                        "else (n)\n"
                                        ":nothing;\n"
                                        "endif\n"
                                        "}\n"
                                        "}\n"
                                        "}\n"
                                        "stop\n");

    for (const qreal width : kWidths)
        expectInvariants(laidOut(text, width), width);
}

TEST(ActivityLayoutTest, NestedPartitionFramesAreInsetOnePerLevel)
{
    const Scene scene = laidOut(QStringLiteral("partition A {\npartition B {\n:x;\n}\n}\n"), 720);

    QList<QRectF> frames = frameRects(scene);
    ASSERT_EQ(frames.size(), 2);
    std::sort(frames.begin(), frames.end(),
              [](const QRectF& a, const QRectF& b) { return a.width() > b.width(); });
    EXPECT_TRUE(inside(frames[0], frames[1]));
    EXPECT_NEAR(frames[1].left() - frames[0].left(), DiagramStyle {}.groupInset, kEpsilon);
    EXPECT_NEAR(frames[0].right() - frames[1].right(), DiagramStyle {}.groupInset, kEpsilon);
}

TEST(ActivityLayoutTest, PlacesAStartAndAStopOnOneAxisJoinedByAnArrow)
{
    const Scene scene = laidOut(QStringLiteral("start\nstop\n"), 360);

    EXPECT_EQ(scene.size, QSizeF(28, 66));
    const QList<SceneEllipse> ellipses = itemsOf<SceneEllipse>(scene);
    ASSERT_EQ(ellipses.size(), 3);
    EXPECT_EQ(firstEllipse(ellipses).rect, QRectF(4, 4, 20, 20));
    EXPECT_EQ(firstEllipse(ellipses).fill, ColorRole::Line);
    EXPECT_EQ(ellipses[1].rect, QRectF(4, 42, 20, 20));
    EXPECT_EQ(ellipses[1].stroke, ColorRole::Line);
    const QList<SceneLine> lines = itemsOf<SceneLine>(scene);
    ASSERT_EQ(lines.size(), 1);
    EXPECT_EQ(lines[0].points, (QList<QPointF> { QPointF(14, 24), QPointF(14, 42) }));
    EXPECT_EQ(lines[0].head, ArrowHead::Filled);
}

TEST(ActivityLayoutTest, DrawsAnEndAsACrossInACircle)
{
    const Scene scene = laidOut(QStringLiteral("start\nend\n"), 360);

    EXPECT_EQ(itemsOf<SceneEllipse>(scene).size(), 2);
    EXPECT_EQ(itemsOf<SceneLine>(scene).size(), 3);
}

TEST(ActivityLayoutTest, DrawsAnActionAsARoundedBoxWithCentredText)
{
    const Scene scene = laidOut(QStringLiteral(":Hello;\n"), 360);

    EXPECT_EQ(scene.size, QSizeF(72, 36));
    const QList<SceneRect> rects = itemsOf<SceneRect>(scene);
    ASSERT_EQ(rects.size(), 1);
    EXPECT_EQ(rects[0].rect, QRectF(4, 4, 64, 28));
    EXPECT_EQ(rects[0].radius, DiagramStyle {}.actionRadius);
    EXPECT_EQ(rects[0].fill, ColorRole::BoxFill);
    const QList<SceneText> texts = itemsOf<SceneText>(scene);
    ASSERT_EQ(texts.size(), 1);
    EXPECT_EQ(texts[0].lines, QStringList { QStringLiteral("Hello") });
    EXPECT_NEAR(texts[0].bounds.center().x(), 36, kEpsilon);
    EXPECT_NEAR(texts[0].bounds.center().y(), 18, kEpsilon);
}

TEST(ActivityLayoutTest, DrawsATitleAboveTheFlowAndWidensForIt)
{
    const Scene scene
        = laidOut(QStringLiteral("title A rather long title for the diagram\n:x;\n"), 360);

    const QList<SceneText> texts = itemsOf<SceneText>(scene);
    ASSERT_EQ(texts.size(), 2);
    EXPECT_EQ(texts[0].lines,
              QStringList { QStringLiteral("A rather long title for the diagram") });
    EXPECT_LT(texts[0].bounds.bottom(), texts[1].bounds.top());
    EXPECT_NEAR(texts[0].bounds.center().x(), scene.size.width() / 2, kEpsilon);
    EXPECT_GT(scene.size.width(), 64 + 8);
}

TEST(ActivityLayoutTest, PutsANoteBesideItsActionOnTheRequestedSide)
{
    const Scene right = laidOut(QStringLiteral(":Do it;\nnote right : Careful\n"), 360);
    const Scene left = laidOut(QStringLiteral(":Do it;\nnote left : Careful\n"), 360);

    for (const bool onRight : { true, false })
    {
        const QList<SceneRect> rects = itemsOf<SceneRect>(onRight ? right : left);
        ASSERT_EQ(rects.size(), 2);
        const SceneRect& action = rects[0];
        const SceneRect& note = rects[1];
        EXPECT_EQ(note.fill, ColorRole::NoteFill);
        EXPECT_EQ(note.stroke, ColorRole::NoteStroke);
        if (onRight)
            EXPECT_GT(note.rect.left(), action.rect.right());
        else
            EXPECT_LT(note.rect.right(), action.rect.left());
        EXPECT_NEAR(note.rect.center().y(), action.rect.center().y(), kEpsilon);
    }
    EXPECT_GT(right.size.width(), 72);
}

TEST(ActivityLayoutTest, AnIfDrawsADiamondPerConditionAndMergesTheBranches)
{
    const Scene scene = laidOut(QStringLiteral("if (a?) then (yes)\n:one;\nelseif (b?) then (no)\n"
                                               ":two;\nelse (rest)\n:three;\nendif\n:after;\n"),
                                360);

    EXPECT_EQ(itemsOf<ScenePolygon>(scene).size(), 2);
    EXPECT_EQ(itemsOf<SceneRect>(scene).size(), 4);
    expectInvariants(scene, 360);
    const QList<SceneRect> rects = itemsOf<SceneRect>(scene);
    const QRectF after = rects.last().rect;
    EXPECT_NEAR(after.center().x(), scene.size.width() / 2, 1.0);
    EXPECT_GT(after.top(), rects[0].rect.bottom());
}

TEST(ActivityLayoutTest, AnIfWithoutElseLeavesAnEmptyColumn)
{
    const Scene without = laidOut(QStringLiteral("if (a?) then (yes)\n:one;\nendif\n"), 360);
    const Scene with
        = laidOut(QStringLiteral("if (a?) then (yes)\n:one;\nelse (no)\n:two;\nendif\n"), 360);

    EXPECT_GT(without.size.width(), 64);
    EXPECT_LT(without.size.width(), with.size.width());
    expectInvariants(without, 360);
}

TEST(ActivityLayoutTest, ABranchThatEndsDrawsNoConnectorAfterIt)
{
    const Scene scene
        = laidOut(QStringLiteral("if (a?) then (yes)\n:one;\ndetach\nelse (no)\n:two;\n"
                                 "endif\n:after;\n"),
                  360);

    qsizetype heads = 0;
    for (const SceneLine& line : itemsOf<SceneLine>(scene))
    {
        if (line.head == ArrowHead::Filled)
            ++heads;
    }
    EXPECT_EQ(heads, 3);
    expectInvariants(scene, 360);
}

TEST(ActivityLayoutTest, ElementsAfterAStopAreDrawnWithoutAConnector)
{
    const Scene scene = laidOut(QStringLiteral(":a;\nstop\n:b;\n"), 360);

    qsizetype heads = 0;
    for (const SceneLine& line : itemsOf<SceneLine>(scene))
    {
        if (line.head == ArrowHead::Filled)
            ++heads;
    }
    EXPECT_EQ(heads, 1);
}

TEST(ActivityLayoutTest, AWhileLoopsBackOnTheRightAndExitsOnTheLeft)
{
    const Scene scene
        = laidOut(QStringLiteral("while (more?) is (yes)\n:work;\nendwhile (no)\n"), 360);

    const QList<ScenePolygon> diamonds = itemsOf<ScenePolygon>(scene);
    ASSERT_EQ(diamonds.size(), 1);
    const QRectF diamond = boundsOf(diamonds[0].points);
    const QList<SceneLine> lines = itemsOf<SceneLine>(scene);
    const SceneLine* back = nullptr;
    const SceneLine* exit = nullptr;
    for (const SceneLine& line : lines)
    {
        if (line.points.size() == 4 && line.head == ArrowHead::Filled)
            back = &line;
        else if (line.points.size() == 4)
            exit = &line;
    }
    ASSERT_NE(back, nullptr);
    ASSERT_NE(exit, nullptr);
    EXPECT_GT(back->points[1].x(), diamond.right());
    EXPECT_NEAR(back->points.last().x(), diamond.right(), kEpsilon);
    EXPECT_LT(exit->points[1].x(), diamond.left());
    EXPECT_NEAR(exit->points.first().x(), diamond.left(), kEpsilon);
    expectInvariants(scene, 360);
}

TEST(ActivityLayoutTest, ARepeatPutsTheConditionBelowTheBody)
{
    const Scene scene
        = laidOut(QStringLiteral("repeat\n:work;\nrepeat while (again?) is (yes) not (no)\n"), 360);

    const QList<ScenePolygon> shapes = itemsOf<ScenePolygon>(scene);
    ASSERT_EQ(shapes.size(), 2);
    const QRectF mark = boundsOf(shapes[0].points);
    const QRectF condition = boundsOf(shapes[1].points);
    const QRectF body = itemsOf<SceneRect>(scene).first().rect;
    EXPECT_LT(mark.bottom(), body.top());
    EXPECT_LT(body.bottom(), condition.top());
    expectInvariants(scene, 360);
}

TEST(ActivityLayoutTest, AForkIsClosedByTwoFilledBarsSpanningTheBranches)
{
    const Scene scene
        = laidOut(QStringLiteral("fork\n:left;\nfork again\n:right;\nend fork\n"), 360);

    QList<SceneRect> bars;
    for (const SceneRect& rect : itemsOf<SceneRect>(scene))
    {
        if (rect.fill == ColorRole::Line)
            bars.append(rect);
    }
    ASSERT_EQ(bars.size(), 2);
    EXPECT_EQ(bars[0].rect.height(), DiagramStyle {}.forkBarHeight);
    EXPECT_NEAR(bars[0].rect.width(), bars[1].rect.width(), kEpsilon);
    EXPECT_LT(bars[0].rect.bottom(), bars[1].rect.top());
    const QList<SceneRect> actions = itemsOf<SceneRect>(scene);
    EXPECT_LT(actions[1].rect.center().x(), actions[2].rect.center().x());
    expectInvariants(scene, 360);
}

TEST(ActivityLayoutTest, APartitionIsAFrameWithATabHoldingItsName)
{
    const Scene scene = laidOut(QStringLiteral("partition Shipping {\n:pack;\n}\n"), 360);

    const QList<QRectF> frames = frameRects(scene);
    ASSERT_EQ(frames.size(), 1);
    const QList<SceneRect> rects = itemsOf<SceneRect>(scene);
    const SceneRect tab = rects[1];
    EXPECT_EQ(tab.fill, ColorRole::BoxFill);
    EXPECT_EQ(tab.stroke, ColorRole::GroupStroke);
    EXPECT_NEAR(tab.rect.left(), frames[0].left(), kEpsilon);
    EXPECT_NEAR(tab.rect.top(), frames[0].top(), kEpsilon);
    EXPECT_TRUE(inside(frames[0], rects.last().rect));
    expectInvariants(scene, 360);
}

TEST(ActivityLayoutTest, LabelsTheConnectorsTheyBelongTo)
{
    const Scene scene = laidOut(QStringLiteral(":a;\n-> after a;\n:b;\n"), 360);

    bool found = false;
    for (const SceneText& text : itemsOf<SceneText>(scene))
    {
        if (text.lines == QStringList { QStringLiteral("after a") })
        {
            found = true;
            EXPECT_EQ(text.color, ColorRole::MutedText);
        }
    }
    EXPECT_TRUE(found);
    expectInvariants(scene, 360);
}

TEST(ActivityLayoutTest, FillsTheSceneMetricsLikeTheSequenceLayout)
{
    const Scene scene = laidOut(QStringLiteral(":a;\n"), 360);

    EXPECT_EQ(scene.fontFamily, DiagramStyle {}.fontFamily);
    EXPECT_EQ(scene.lineHeight, FakeTextMeasurer::kLineHeight);
    EXPECT_EQ(scene.ascent, FakeTextMeasurer::kAscent);
    EXPECT_EQ(scene.arrowHeadSize, DiagramStyle {}.arrowHeadSize);
    EXPECT_EQ(scene.size.width(), std::floor(scene.size.width()));
    EXPECT_EQ(scene.size.height(), std::floor(scene.size.height()));
}

TEST(ActivityLayoutTest, ABranchThatNeedsMoreWidthGetsClearlyMoreThanASimpleOne)
{
    for (const qreal width : { 352.0, 400.0 })
    {
        const Scene scene = laidOut(kNestedIf, width);
        const QRectF allocate
            = nodeAround(scene, QStringLiteral("Allocate a physical page and map it"));
        const QRectF send = nodeAround(scene, QStringLiteral("Send SIGSEGV"));
        const QRectF simple = nodeAround(scene, QStringLiteral("Send SIGSEGV to the process"));

        const qreal nestedSpan = send.right() - allocate.left();
        EXPECT_GT(nestedSpan, 1.5 * simple.width()) << width;
        expectInvariants(scene, width);
    }
}

TEST(ActivityLayoutTest, AForkBranchWithANoteGetsMoreThanAOneActionBranch)
{
    for (const qreal width : { 352.0, 400.0 })
    {
        const Scene scene = laidOut(kForkWithNote, width);
        const QRectF copy = nodeAround(scene, QStringLiteral("Copy the data to the buffer"));
        const QRectF wake = nodeAround(scene, QStringLiteral("Wake the waiting reader"));
        const QRectF note = nodeAround(scene, QStringLiteral("runs with interrupts enabled"));

        EXPECT_GT(copy.right() - note.left(), 1.5 * wake.width()) << width;
        expectInvariants(scene, width);
    }
}

TEST(ActivityLayoutTest, ForkBarsCoverExactlyTheBranchesWithPadding)
{
    for (const qreal width : { 352.0, 720.0 })
    {
        const Scene scene = laidOut(kForkWithNote, width);
        const QList<SceneRect> bars = forkBars(scene);
        ASSERT_EQ(bars.size(), 2);
        const QRectF note = nodeAround(scene, QStringLiteral("runs with interrupts enabled"));
        const QRectF wake = nodeAround(scene, QStringLiteral("Wake the waiting reader"));
        const qreal padding = DiagramStyle {}.padding;

        for (const SceneRect& bar : bars)
        {
            EXPECT_NEAR(bar.rect.left(), note.left() - padding, kEpsilon) << width;
            EXPECT_NEAR(bar.rect.right(), wake.right() + padding, kEpsilon) << width;
        }
        const QList<QRectF> frames = frameRects(scene);
        ASSERT_EQ(frames.size(), 1);
        EXPECT_GT(bars[0].rect.left(), frames[0].left());
        EXPECT_LT(bars[0].rect.right(), frames[0].right());
    }
}

TEST(ActivityLayoutTest, AForkBranchWithANoteDoesNotStarveTheNextBranch)
{
    const Scene scene = laidOut(kForkWithNote, 352);
    const QRectF copy = nodeAround(scene, QStringLiteral("Copy the data to the buffer"));
    const QRectF wake = nodeAround(scene, QStringLiteral("Wake the waiting reader"));
    const QRectF note = nodeAround(scene, QStringLiteral("runs with interrupts enabled"));

    EXPECT_LE(lineCountOf(scene, QStringLiteral("Wake the waiting reader")), 3);
    EXPECT_LE(lineCountOf(scene, QStringLiteral("Copy the data to the buffer")), 3);
    EXPECT_LT(wake.left() - copy.right(), copy.width() / 2);
    EXPECT_LT(copy.left() - note.right(), 2 * DiagramStyle {}.columnGap + kEpsilon);
    expectInvariants(scene, 352);
}

TEST(ActivityLayoutTest, AForkBranchGetsTwoLinesOnceTheWidthAllowsIt)
{
    const Scene scene = laidOut(kForkWithNote, 480);

    EXPECT_LE(lineCountOf(scene, QStringLiteral("Wake the waiting reader")), 2);
    expectInvariants(scene, 480);
}

TEST(ActivityLayoutTest, AnIfColumnWithANoteIsWiderThanItsShortNeighbour)
{
    const QString text = QStringLiteral("if (Ready?) then (yes)\n"
                                        "  :Process the request in the worker;\n"
                                        "  note right : The worker retries three times\n"
                                        "else (no)\n"
                                        "  :Skip;\n"
                                        "endif\n");

    for (const qreal width : { 240.0, 320.0, 400.0, 480.0, 720.0 })
    {
        const Scene scene = laidOut(text, width);
        const QRectF process
            = nodeAround(scene, QStringLiteral("Process the request in the worker"));
        const QRectF note = nodeAround(scene, QStringLiteral("The worker retries three times"));
        const QRectF skip = nodeAround(scene, QStringLiteral("Skip"));

        EXPECT_FALSE(overlaps(process, skip)) << width;
        EXPECT_FALSE(overlaps(note, skip)) << width;
        EXPECT_GT(note.right() - process.left(), 2 * skip.width()) << width;
        EXPECT_LE(scene.size.width(), width + kEpsilon) << width;
        expectInvariants(scene, width);
    }
}

TEST(ActivityLayoutTest, ATopLevelActionWithALeftNoteStaysInsideTheScene)
{
    const QString text = QStringLiteral(":Flush the write buffer to the disk;\n"
                                        "note left : Blocks until the disk confirms the write\n");
    const qreal minimum = laidOut(text, 0).size.width();

    for (const qreal width : kWidths)
    {
        const Scene scene = laidOut(text, width);
        const QRectF action
            = nodeAround(scene, QStringLiteral("Flush the write buffer to the disk"));
        const QRectF note
            = nodeAround(scene, QStringLiteral("Blocks until the disk confirms the write"));

        EXPECT_FALSE(overlaps(action, note)) << width;
        EXPECT_LT(note.right(), action.left() + kEpsilon) << width;
        EXPECT_LE(scene.size.width(), std::max(width, minimum) + kEpsilon) << width;
        expectInvariants(scene, width);
    }
}

TEST(ActivityLayoutTest, IsDeterministic)
{
    const Scene first = laidOut(kRichDiagram, 360);
    const Scene second = laidOut(kRichDiagram, 360);

    EXPECT_EQ(first.size, second.size);
    EXPECT_EQ(first.items.size(), second.items.size());
    EXPECT_EQ(SvgSceneWriter::write(first, DiagramPalette {}),
              SvgSceneWriter::write(second, DiagramPalette {}));
}

TEST(ActivityLayoutTest, StaysFastOnADeeplyNestedDiagram)
{
    QString text;
    for (int level = 0; level < 12; ++level)
        text += QStringLiteral("if (condition %1?) then (yes)\n").arg(level);
    text += QStringLiteral(":core;\n");
    for (int level = 0; level < 12; ++level)
        text += QStringLiteral("else (no)\n:other %1;\nendif\n").arg(level);

    const Scene scene = laidOut(text, 360);

    EXPECT_GT(scene.size.height(), 0);
}
