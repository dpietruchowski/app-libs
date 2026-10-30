#include <gtest/gtest.h>

#include "diagram/blocklayout.h"
#include "diagram/blockparser.h"
#include "diagram/svgscenewriter.h"
#include "faketextmeasurer.h"

#include <algorithm>

using namespace diagram;

namespace
{
constexpr qreal kEpsilon = 0.01;

const QString kSocDiagram = QStringLiteral("rectangle \"SoC\" {\n"
                                           "  rectangle \"CPU\" as CPU\n"
                                           "  rectangle \"Boot ROM\" as ROM\n"
                                           "  rectangle \"SRAM\\nsmall, ready at once\" as SRAM\n"
                                           "  rectangle \"DDR controller\" as CTRL\n"
                                           "}\n"
                                           "rectangle \"external DDR\" as DDR\n"
                                           "CPU -- ROM\n"
                                           "CPU -- SRAM\n"
                                           "CPU -- CTRL\n"
                                           "CTRL -- DDR\n");

const QString kRichDiagram = QStringLiteral("title Memory map of the board\n"
                                            "rectangle \"Board\" {\n"
                                            "  rectangle \"SoC\" {\n"
                                            "    component \"CPU core\" as CPU\n"
                                            "    node \"Boot ROM\" as ROM\n"
                                            "  }\n"
                                            "  database \"Flash storage\" as FLASH\n"
                                            "}\n"
                                            "rectangle \"External DDR memory\" as DDR\n"
                                            "CPU --> ROM : fetches the first instructions\n"
                                            "CPU .. FLASH : loads the kernel\n"
                                            "ROM <-- DDR\n"
                                            "note left of CPU : runs at one gigahertz\n"
                                            "note right of DDR : slow but large\n");

const QList<qreal> kWidths { 200, 240, 280, 320, 360, 400, 480, 560, 640 };

BlockDiagram parsed(const QString& text)
{
    const auto result = BlockParser::parse(text);
    EXPECT_TRUE(result.isSuccess()) << (result.isFailure() ? result.error().toStdString() : "");
    return result.isSuccess() ? result.value() : BlockDiagram {};
}

Scene laidOut(const QString& text, qreal width)
{
    static const FakeTextMeasurer measurer;
    return BlockLayout::layout(parsed(text), DiagramStyle {}, width, measurer);
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

QList<QRectF> filledRects(const Scene& scene)
{
    QList<QRectF> rects;
    for (const SceneRect& rect : itemsOf<SceneRect>(scene))
    {
        if (rect.fill && rect.fill != ColorRole::Background && rect.stroke)
            rects.append(rect.rect);
    }
    return rects;
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

QList<SceneLine> linesWithEndpoints(const Scene& scene)
{
    QList<SceneLine> lines;
    for (const SceneLine& line : itemsOf<SceneLine>(scene))
    {
        if (line.color == ColorRole::Line)
            lines.append(line);
    }
    return lines;
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

    const QList<SceneText> texts = itemsOf<SceneText>(scene);
    for (qsizetype i = 0; i < texts.size(); ++i)
    {
        const std::string shown = texts[i].lines.join(QLatin1Char(' ')).toStdString();
        EXPECT_TRUE(inside(area, texts[i].bounds)) << availableWidth << " " << shown;
        for (const QString& line : texts[i].lines)
            EXPECT_LE(FakeTextMeasurer().width(line), texts[i].bounds.width() + kEpsilon);
        for (qsizetype j = i + 1; j < texts.size(); ++j)
            EXPECT_FALSE(overlaps(texts[i].bounds, texts[j].bounds))
                << availableWidth << " " << shown << " / "
                << texts[j].lines.join(QLatin1Char(' ')).toStdString();
    }

    const QList<QRectF> boxes = filledRects(scene);
    for (qsizetype i = 0; i < boxes.size(); ++i)
    {
        for (qsizetype j = i + 1; j < boxes.size(); ++j)
        {
            const bool nested = boxes[i].contains(boxes[j]) || boxes[j].contains(boxes[i]);
            if (!nested)
                EXPECT_FALSE(overlaps(boxes[i], boxes[j])) << availableWidth;
        }
    }
}
}

TEST(BlockLayoutTest, TheSocExampleFitsAPhoneWidth)
{
    const Scene scene = laidOut(kSocDiagram, 360);

    EXPECT_LE(scene.size.width(), 360);
    expectInvariants(scene, 360);
    EXPECT_EQ(scene.fontPixelSize, DiagramStyle {}.fontPixelSize);
}

TEST(BlockLayoutTest, TheContainerEnclosesItsChildrenAndNotTheOutsideBlock)
{
    const Scene scene = laidOut(kSocDiagram, 360);

    const QList<QRectF> frames = frameRects(scene);
    ASSERT_EQ(frames.size(), 1);
    const QRectF frame = frames.first();
    for (const QString& label :
         { QStringLiteral("CPU"), QStringLiteral("Boot ROM"),
           QStringLiteral("SRAM small, ready at once"), QStringLiteral("DDR controller") })
        EXPECT_TRUE(inside(frame, textBounds(scene, label))) << label.toStdString();
    EXPECT_TRUE(inside(frame, textBounds(scene, QStringLiteral("SoC"))));
    EXPECT_FALSE(overlaps(frame, textBounds(scene, QStringLiteral("external DDR"))));
    EXPECT_GT(textBounds(scene, QStringLiteral("external DDR")).top(), frame.bottom());
}

TEST(BlockLayoutTest, LinkedBlocksSitInRowsInTheOrderOfTheLinks)
{
    const Scene scene = laidOut(kSocDiagram, 360);

    const QRectF cpu = textBounds(scene, QStringLiteral("CPU"));
    EXPECT_LT(cpu.bottom(), textBounds(scene, QStringLiteral("Boot ROM")).top());
    EXPECT_LT(cpu.bottom(), textBounds(scene, QStringLiteral("SRAM small, ready at once")).top());
    EXPECT_LT(cpu.bottom(), textBounds(scene, QStringLiteral("DDR controller")).top());
}

TEST(BlockLayoutTest, EveryLinkJoinsTheEdgesOfItsTwoBlocks)
{
    const Scene scene = laidOut(kSocDiagram, 360);

    const QList<SceneLine> lines = linesWithEndpoints(scene);
    ASSERT_EQ(lines.size(), 4);
    const QList<QRectF> boxes = filledRects(scene);
    for (const SceneLine& line : lines)
    {
        for (const QPointF& end : { line.points.first(), line.points.last() })
        {
            const bool onABoxEdge
                = std::any_of(boxes.cbegin(), boxes.cend(),
                              [&end](const QRectF& box)
                              {
                                  const bool onHorizontalEdge
                                      = std::abs(end.y() - box.top()) < kEpsilon
                                      || std::abs(end.y() - box.bottom()) < kEpsilon;
                                  return onHorizontalEdge && end.x() >= box.left() - kEpsilon
                                      && end.x() <= box.right() + kEpsilon;
                              });
            EXPECT_TRUE(onABoxEdge);
        }
    }
}

TEST(BlockLayoutTest, NoLinkRunsThroughABlock)
{
    for (const qreal width : kWidths)
    {
        const Scene scene = laidOut(kSocDiagram, width);
        const QList<QRectF> boxes = filledRects(scene);
        for (const SceneLine& line : linesWithEndpoints(scene))
        {
            for (qsizetype index = 1; index < line.points.size(); ++index)
            {
                const QRectF segment
                    = boundsOf({ line.points.at(index - 1), line.points.at(index) });
                for (const QRectF& box : boxes)
                    EXPECT_FALSE(overlaps(segment.adjusted(-0.4, -0.4, 0.4, 0.4),
                                          box.adjusted(1, 1, -1, -1)))
                        << width;
            }
        }
    }
}

TEST(BlockLayoutTest, KeepsTheInvariantsAtEveryWidth)
{
    for (const qreal width : kWidths)
    {
        const Scene scene = laidOut(kSocDiagram, width);
        expectInvariants(scene, width);
        EXPECT_LE(scene.size.width(), width);
    }
}

TEST(BlockLayoutTest, KeepsTheInvariantsOfARichDiagramAtEveryWidth)
{
    for (const qreal width : kWidths)
    {
        const Scene scene = laidOut(kRichDiagram, width);
        expectInvariants(scene, width);
        if (width >= 320)
            EXPECT_LE(scene.size.width(), width) << width;
    }
}

TEST(BlockLayoutTest, ANarrowerWidthWrapsTextAndKeepsTheFontSize)
{
    const auto lineCount = [](const Scene& scene)
    {
        qsizetype count = 0;
        for (const SceneText& text : itemsOf<SceneText>(scene))
            count += text.lines.size();
        return count;
    };

    const Scene wide = laidOut(kSocDiagram, 640);
    const Scene narrow = laidOut(kSocDiagram, 240);

    EXPECT_GT(lineCount(narrow), lineCount(wide));
    EXPECT_EQ(narrow.fontPixelSize, wide.fontPixelSize);
}

TEST(BlockLayoutTest, DrawsEachKindOfBlockInItsOwnShape)
{
    const Scene scene
        = laidOut(QStringLiteral("rectangle R\nnode N\ncomponent C\ndatabase D\n"), 640);

    EXPECT_EQ(itemsOf<ScenePolygon>(scene).size(), 2);
    EXPECT_EQ(itemsOf<SceneEllipse>(scene).size(), 2);
    EXPECT_EQ(itemsOf<SceneLine>(scene).size(), 2);
    EXPECT_EQ(filledRects(scene).size(), 1 + 1 + 4);
    expectInvariants(scene, 640);
}

TEST(BlockLayoutTest, DrawsOnlyWhatTheWriterCanWrite)
{
    const QString svg = SvgSceneWriter::write(laidOut(kRichDiagram, 360), DiagramPalette {}, 1);

    EXPECT_TRUE(svg.contains(QStringLiteral("<polygon")));
    EXPECT_TRUE(svg.contains(QStringLiteral("<ellipse")));
    EXPECT_TRUE(svg.contains(QStringLiteral("stroke-dasharray")));
}

TEST(BlockLayoutTest, LinksHaveHeadsAndDashesAsWritten)
{
    const Scene scene = laidOut(QStringLiteral("rectangle A\nrectangle B\nrectangle C\n"
                                               "rectangle D\nA -- B\nA --> C\nD <-- A\nB .. C\n"),
                                360);

    const QList<SceneLine> lines = linesWithEndpoints(scene);
    ASSERT_EQ(lines.size(), 4);
    EXPECT_EQ(lines.at(0).head, ArrowHead::None);
    EXPECT_FALSE(lines.at(0).dashed);
    EXPECT_EQ(lines.at(1).head, ArrowHead::Filled);
    EXPECT_EQ(lines.at(2).head, ArrowHead::Filled);
    EXPECT_TRUE(lines.at(3).dashed);
    EXPECT_EQ(lines.at(3).head, ArrowHead::None);
}

TEST(BlockLayoutTest, ALinkLabelSitsBetweenTheBlocksAndTouchesNone)
{
    const Scene scene = laidOut(QStringLiteral("rectangle A\nrectangle B\nA --> B : reads\n"), 360);

    const QRectF label = textBounds(scene, QStringLiteral("reads"));
    EXPECT_GT(label.top(), textBounds(scene, QStringLiteral("A")).bottom());
    EXPECT_LT(label.bottom(), textBounds(scene, QStringLiteral("B")).top());
    for (const QRectF& box : filledRects(scene))
        EXPECT_FALSE(overlaps(label, box));
}

TEST(BlockLayoutTest, NotesSitBesideTheirBlockOnTheRequestedSide)
{
    const Scene scene = laidOut(QStringLiteral("rectangle A\nrectangle B\n"
                                               "note left of A : on the left\n"
                                               "note right of B : on the right\n"),
                                480);

    const QRectF left = textBounds(scene, QStringLiteral("on the left"));
    const QRectF right = textBounds(scene, QStringLiteral("on the right"));
    EXPECT_LT(left.right(), textBounds(scene, QStringLiteral("A")).left());
    EXPECT_GT(right.left(), textBounds(scene, QStringLiteral("B")).right());
    EXPECT_EQ(itemsOf<SceneRect>(scene).size(), 4);
    expectInvariants(scene, 480);
}

TEST(BlockLayoutTest, TheTitleSitsAboveTheDiagram)
{
    const Scene scene = laidOut(QStringLiteral("title The board\nrectangle A\n"), 360);

    EXPECT_LT(textBounds(scene, QStringLiteral("The board")).bottom(),
              textBounds(scene, QStringLiteral("A")).top());
}

TEST(BlockLayoutTest, ABlockWithoutLinksFlowsIntoRowsThatFitTheWidth)
{
    const Scene scene = laidOut(QStringLiteral("rectangle \"First block\" as A\n"
                                               "rectangle \"Second block\" as B\n"
                                               "rectangle \"Third block\" as C\n"
                                               "rectangle \"Fourth block\" as D\n"),
                                360);

    EXPECT_LE(scene.size.width(), 360);
    EXPECT_EQ(textBounds(scene, QStringLiteral("First block")).top(),
              textBounds(scene, QStringLiteral("Second block")).top());
    expectInvariants(scene, 360);
}

TEST(BlockLayoutTest, ALongLinkGoesAroundTheBlockInItsWay)
{
    const Scene scene = laidOut(QStringLiteral("rectangle A\nrectangle B\nrectangle C\n"
                                               "A -- B\nB -- C\nA -- C\n"),
                                360);

    const QList<QRectF> boxes = filledRects(scene);
    ASSERT_EQ(boxes.size(), 3);
    for (const SceneLine& line : linesWithEndpoints(scene))
    {
        for (qsizetype index = 1; index < line.points.size(); ++index)
        {
            const QRectF segment = boundsOf({ line.points.at(index - 1), line.points.at(index) });
            for (const QRectF& box : boxes)
                EXPECT_FALSE(
                    overlaps(segment.adjusted(-0.4, -0.4, 0.4, 0.4), box.adjusted(1, 1, -1, -1)));
        }
    }
    expectInvariants(scene, 360);
}

TEST(BlockLayoutTest, ALinkBetweenAContainerAndItsChildStaysInsideTheContainer)
{
    const Scene scene = laidOut(QStringLiteral("rectangle \"Group\" as G {\n"
                                               "  rectangle Inner\n"
                                               "}\n"
                                               "G -- Inner\n"),
                                360);

    const QList<QRectF> frames = frameRects(scene);
    ASSERT_EQ(frames.size(), 1);
    const QList<SceneLine> lines = linesWithEndpoints(scene);
    ASSERT_EQ(lines.size(), 1);
    for (const QPointF& point : lines.first().points)
        EXPECT_TRUE(inside(frames.first(), QRectF(point, QSizeF(0, 0))));
}

TEST(BlockLayoutTest, SamePieceOfTextAlwaysGivesTheSameScene)
{
    const Scene first = laidOut(kRichDiagram, 360);
    const Scene second = laidOut(kRichDiagram, 360);

    EXPECT_EQ(first.size, second.size);
    EXPECT_EQ(first.items.size(), second.items.size());
}
