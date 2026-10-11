#include <gtest/gtest.h>

#include "diagram/bitslayout.h"
#include "diagram/bitsparser.h"
#include "diagram/diagramlayout.h"
#include "diagram/diagramparser.h"
#include "faketextmeasurer.h"

#include <QSet>

#include <algorithm>
#include <optional>

using namespace diagram;

namespace
{
constexpr qreal kEpsilon = 0.01;

const QString kPacket = QStringLiteral("title struct Packet (12 B)\n"
                                       "bytes 4\n"
                                       "field  0..0   char tag\n"
                                       "pad    1..3\n"
                                       "field  4..7   int  id\n"
                                       "field  8..9   short len\n"
                                       "pad    10..11\n");

const QString kEntry = QStringLiteral("title PTE (x86-64)\n"
                                      "bits 64\n"
                                      "field 0      P   \"present\"\n"
                                      "field 1      RW\n"
                                      "field 12..51 PFN \"frame number\"\n"
                                      "field 63     NX\n");

const QString kCrowded = QStringLiteral("title A long title for a small register\n"
                                        "bytes 16\n"
                                        "field 0..1 sequence_number \"counts every frame sent\"\n"
                                        "field 2 flags\n"
                                        "field 3..20 a_long_payload_name \"free form\"\n"
                                        "pad 21..22\n"
                                        "field 40..47 trailer_checksum_value\n");

const QList<qreal> kWidths { 120, 200, 264, 280, 320, 360, 400, 520, 640, 800, 1100 };

BitsDiagram parsed(const QString& text)
{
    const auto result = BitsParser::parse(text);
    EXPECT_TRUE(result.isSuccess()) << (result.isFailure() ? result.error().toStdString() : "");
    return result.isSuccess() ? result.value() : BitsDiagram {};
}

Scene laidOut(const QString& text, qreal width)
{
    static const FakeTextMeasurer measurer;
    return BitsLayout::layout(parsed(text), DiagramStyle {}, width, measurer);
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

bool overlaps(const QRectF& a, const QRectF& b)
{
    const QRectF common = a.intersected(b);
    return common.width() > kEpsilon && common.height() > kEpsilon;
}

bool inside(const QRectF& outer, const QRectF& inner)
{
    return inner.left() >= outer.left() - kEpsilon && inner.top() >= outer.top() - kEpsilon
        && inner.right() <= outer.right() + kEpsilon && inner.bottom() <= outer.bottom() + kEpsilon;
}

int rowCount(const Scene& scene)
{
    QSet<qreal> tops;
    for (const SceneRect& rect : itemsOf<SceneRect>(scene))
        tops.insert(rect.rect.top());
    return int(tops.size());
}

std::optional<SceneText> textLine(const Scene& scene, const QString& line)
{
    for (const SceneText& text : itemsOf<SceneText>(scene))
    {
        if (text.lines.contains(line))
            return text;
    }
    return std::nullopt;
}

QList<SceneRect> boxesFilled(const Scene& scene, ColorRole fill)
{
    QList<SceneRect> boxes;
    for (const SceneRect& rect : itemsOf<SceneRect>(scene))
    {
        if (rect.fill == fill)
            boxes.append(rect);
    }
    return boxes;
}

QRectF boxContaining(const Scene& scene, const QRectF& bounds)
{
    for (const SceneRect& rect : itemsOf<SceneRect>(scene))
    {
        if (rect.fill == ColorRole::BoxFill && inside(rect.rect, bounds))
            return rect.rect;
    }
    return QRectF();
}

int dashedLines(const Scene& scene)
{
    const QList<SceneLine> lines = itemsOf<SceneLine>(scene);
    return int(std::count_if(lines.cbegin(), lines.cend(),
                             [](const SceneLine& line) { return line.dashed; }));
}

QRectF boundsOf(const SceneLine& line)
{
    return QRectF(line.points.first(), line.points.last()).normalized();
}

void expectInvariants(const Scene& scene, qreal width)
{
    const QRectF whole(QPointF(0, 0), scene.size);
    const QList<SceneText> texts = itemsOf<SceneText>(scene);
    for (const SceneRect& rect : itemsOf<SceneRect>(scene))
        EXPECT_TRUE(inside(whole, rect.rect)) << "width " << width;
    for (const SceneLine& line : itemsOf<SceneLine>(scene))
    {
        for (const QPointF& point : line.points)
            EXPECT_TRUE(inside(whole, QRectF(point, QSizeF(0, 0)))) << "width " << width;
    }
    for (qsizetype first = 0; first < texts.size(); ++first)
    {
        EXPECT_TRUE(inside(whole, texts.at(first).bounds)) << "width " << width;
        for (qsizetype second = first + 1; second < texts.size(); ++second)
            EXPECT_FALSE(overlaps(texts.at(first).bounds, texts.at(second).bounds))
                << "width " << width << ": " << texts.at(first).lines.join('|').toStdString()
                << " and " << texts.at(second).lines.join('|').toStdString();
    }
    EXPECT_GT(scene.size.height(), 0);
}
}

TEST(BitsLayoutTest, ARegisterThatFitsIsOneRowWithTheHighBitOnTheLeft)
{
    const Scene scene = laidOut(QStringLiteral("bits 8\nfield 4..7 hi\nfield 0..3 lo\n"), 400);

    EXPECT_EQ(rowCount(scene), 1);
    const auto high = textLine(scene, QStringLiteral("hi"));
    const auto low = textLine(scene, QStringLiteral("lo"));
    ASSERT_TRUE(high && low);
    EXPECT_LT(high->bounds.right(), low->bounds.left());
    EXPECT_EQ(high->bounds.top(), low->bounds.top());
}

TEST(BitsLayoutTest, BitNumbersStandOnTheBoundariesOfEveryBoxAboveIt)
{
    const Scene scene = laidOut(QStringLiteral("bits 8\nfield 4..7 hi\nfield 2 mid\n"), 400);

    for (const QString& number : { QStringLiteral("7"), QStringLiteral("4"), QStringLiteral("3"),
                                   QStringLiteral("2"), QStringLiteral("1"), QStringLiteral("0") })
    {
        const auto text = textLine(scene, number);
        ASSERT_TRUE(text) << number.toStdString();
        bool aboveABox = false;
        for (const SceneRect& rect : itemsOf<SceneRect>(scene))
        {
            aboveABox = aboveABox
                || (rect.rect.left() <= text->bounds.center().x()
                    && text->bounds.center().x() <= rect.rect.right()
                    && text->bounds.bottom() <= rect.rect.top() + kEpsilon
                    && rect.rect.top() - text->bounds.bottom() < 8);
        }
        EXPECT_TRUE(aboveABox) << number.toStdString();
    }
    EXPECT_FALSE(textLine(scene, QStringLiteral("5")));
    EXPECT_FALSE(textLine(scene, QStringLiteral("6")));
}

TEST(BitsLayoutTest, ANarrowWidthWrapsTheRegisterIntoRowsOf32Then16Then8Bits)
{
    const QString text = QStringLiteral("bits 64\nfield 0..63 value\n");

    EXPECT_EQ(rowCount(laidOut(text, 1100)), 1);
    EXPECT_EQ(rowCount(laidOut(text, 800)), 2);
    EXPECT_EQ(rowCount(laidOut(text, 520)), 2);
    EXPECT_EQ(rowCount(laidOut(text, 519)), 4);
    EXPECT_EQ(rowCount(laidOut(text, 264)), 4);
    EXPECT_EQ(rowCount(laidOut(text, 263)), 8);
    EXPECT_EQ(rowCount(laidOut(text, 120)), 8);
}

TEST(BitsLayoutTest, ASmallRegisterNeverWrapsToWiderRows)
{
    EXPECT_EQ(rowCount(laidOut(QStringLiteral("bits 24\nfield 0..23 v\n"), 800)), 1);
    EXPECT_EQ(rowCount(laidOut(QStringLiteral("bits 24\nfield 0..23 v\n"), 300)), 2);
    EXPECT_EQ(rowCount(laidOut(QStringLiteral("bits 8\nfield 0..7 v\n"), 60)), 1);
    EXPECT_EQ(rowCount(laidOut(QStringLiteral("bits 12\nfield 0..11 v\n"), 100)), 2);
}

TEST(BitsLayoutTest, WrappedRowsKeepTheBitColumnsAndGiveEveryRowItsOwnNumbers)
{
    const Scene scene = laidOut(QStringLiteral("bits 64\nfield 0..63 value\n"), 300);

    EXPECT_TRUE(textLine(scene, QStringLiteral("63")));
    EXPECT_TRUE(textLine(scene, QStringLiteral("48")));
    EXPECT_TRUE(textLine(scene, QStringLiteral("47")));
    EXPECT_TRUE(textLine(scene, QStringLiteral("16")));
    EXPECT_TRUE(textLine(scene, QStringLiteral("15")));
    EXPECT_TRUE(textLine(scene, QStringLiteral("0")));
    const auto top = textLine(scene, QStringLiteral("63"));
    const auto bottom = textLine(scene, QStringLiteral("15"));
    EXPECT_LT(top->bounds.top(), bottom->bounds.top());
    EXPECT_NEAR(top->bounds.center().x(), bottom->bounds.center().x(), kEpsilon);
}

TEST(BitsLayoutTest, APartialTopRowKeepsTheColumnsOfItsBits)
{
    const Scene scene = laidOut(QStringLiteral("bits 12\nfield 0..11 v\n"), 100);

    const auto eleven = textLine(scene, QStringLiteral("11"));
    const auto seven = textLine(scene, QStringLiteral("7"));
    const auto eight = textLine(scene, QStringLiteral("8"));
    const auto zero = textLine(scene, QStringLiteral("0"));
    ASSERT_TRUE(eleven && seven && eight && zero);
    EXPECT_LT(eleven->bounds.top(), seven->bounds.top());
    EXPECT_GT(eleven->bounds.center().x(), seven->bounds.center().x());
    EXPECT_NEAR(eight->bounds.center().x(), zero->bounds.center().x(), kEpsilon);
}

TEST(BitsLayoutTest, AFieldAcrossARowEdgeIsOneBoxPerRowWithDashedCutEdges)
{
    const Scene scene = laidOut(QStringLiteral("bits 32\nfield 8..23 wide\nfield 0..7 low\n"
                                               "field 24..31 high\n"),
                                300);

    EXPECT_EQ(rowCount(scene), 2);
    EXPECT_EQ(boxesFilled(scene, ColorRole::BoxFill).size(), 4);
    EXPECT_EQ(dashedLines(scene), 2);
    EXPECT_TRUE(textLine(scene, QStringLiteral("wide")));
    EXPECT_TRUE(textLine(scene, QStringLiteral("wide (cont.)")));
}

TEST(BitsLayoutTest, UndeclaredBitsAndPaddingAreHatchedAndCarryNoText)
{
    const Scene scene = laidOut(QStringLiteral("bits 16\nfield 8..15 hi\npad 4..5\n"), 600);

    const QList<SceneRect> background = boxesFilled(scene, ColorRole::Background);
    ASSERT_EQ(background.size(), 3);
    EXPECT_GT(itemsOf<SceneLine>(scene).size(), 6);
    for (const SceneLine& line : itemsOf<SceneLine>(scene))
    {
        bool onAHatchedBox = false;
        for (const SceneRect& rect : background)
            onAHatchedBox = onAHatchedBox || inside(rect.rect, boundsOf(line));
        EXPECT_TRUE(onAHatchedBox);
    }
}

TEST(BitsLayoutTest, ADescriptionSitsBelowItsFieldInSmallerText)
{
    const Scene scene = laidOut(kEntry, 800);

    const auto description = textLine(scene, QStringLiteral("frame number"));
    ASSERT_TRUE(description);
    EXPECT_LT(description->sizeFactor, 1);
    const auto label = textLine(scene, QStringLiteral("PFN"));
    ASSERT_TRUE(label);
    EXPECT_EQ(label->sizeFactor, 1);
    EXPECT_GT(description->bounds.top(), label->bounds.bottom());
    const QRectF box = boxContaining(scene, label->bounds);
    EXPECT_GE(description->bounds.top(), box.bottom());
    EXPECT_NEAR(description->bounds.center().x(), box.center().x(), kEpsilon);
}

TEST(BitsLayoutTest, ALabelOrDescriptionThatDoesNotFitMovesToTheListBelow)
{
    const Scene scene = laidOut(QStringLiteral("bits 8\nfield 0 a_long_label "
                                               "\"unbreakable_description\"\nfield 1..7 ok\n"),
                                400);

    EXPECT_FALSE(textLine(scene, QStringLiteral("a_long_label")));
    EXPECT_FALSE(textLine(scene, QStringLiteral("unbreakable_description")));
    const auto listed
        = textLine(scene, QStringLiteral("[0] a_long_label - unbreakable_description"));
    ASSERT_TRUE(listed);
    const auto fitted = textLine(scene, QStringLiteral("ok"));
    ASSERT_TRUE(fitted);
    EXPECT_GT(listed->bounds.top(), fitted->bounds.bottom());
    EXPECT_LT(listed->sizeFactor, 1);
    EXPECT_TRUE(inside(QRectF(QPointF(0, 0), scene.size), listed->bounds));
}

TEST(BitsLayoutTest, ADescriptionThatFitsStaysUnderItsFieldAndOutOfTheList)
{
    const Scene scene = laidOut(QStringLiteral("bits 8\nfield 0..7 x \"short\"\n"), 400);

    EXPECT_TRUE(textLine(scene, QStringLiteral("short")));
    for (const SceneText& text : itemsOf<SceneText>(scene))
        EXPECT_FALSE(text.lines.first().startsWith(QLatin1Char('[')));
}

TEST(BitsLayoutTest, ABytesLayoutIsAGridWithTheOffsetOfEveryRowOnTheLeft)
{
    const Scene scene = laidOut(kPacket, 360);

    EXPECT_EQ(rowCount(scene), 3);
    for (const QString& offset : { QStringLiteral("0"), QStringLiteral("4"), QStringLiteral("8") })
    {
        const auto text = textLine(scene, offset);
        ASSERT_TRUE(text) << offset.toStdString();
        EXPECT_EQ(text->align, Qt::AlignRight);
    }
    const auto tag = textLine(scene, QStringLiteral("char tag"));
    const auto id = textLine(scene, QStringLiteral("int id"));
    const auto len = textLine(scene, QStringLiteral("short len"));
    ASSERT_TRUE(tag && id && len);
    EXPECT_LT(tag->bounds.top(), id->bounds.top());
    EXPECT_LT(id->bounds.top(), len->bounds.top());
    EXPECT_LT(textLine(scene, QStringLiteral("0"))->bounds.right(),
              boxContaining(scene, tag->bounds).left());
}

TEST(BitsLayoutTest, BytesGoLeftToRightWithTheLowestAddressTopLeft)
{
    const Scene scene = laidOut(QStringLiteral("bytes 4\nfield 0 a\nfield 1 b\nfield 4 c\n"), 360);

    const auto a = textLine(scene, QStringLiteral("a"));
    const auto b = textLine(scene, QStringLiteral("b"));
    const auto c = textLine(scene, QStringLiteral("c"));
    ASSERT_TRUE(a && b && c);
    EXPECT_LT(a->bounds.left(), b->bounds.left());
    EXPECT_EQ(a->bounds.top(), b->bounds.top());
    EXPECT_NEAR(a->bounds.center().x(), c->bounds.center().x(), kEpsilon);
    EXPECT_GT(c->bounds.top(), a->bounds.top());
}

TEST(BitsLayoutTest, APadIsHatchedAndUndeclaredBytesAreAnEmptyDashedOutline)
{
    const Scene scene = laidOut(QStringLiteral("bytes 4\nfield 0 a\npad 1\nfield 4 b\n"), 360);

    int outlines = 0;
    for (const SceneRect& rect : itemsOf<SceneRect>(scene))
    {
        if (!rect.fill && rect.dashed)
            ++outlines;
    }
    EXPECT_EQ(outlines, 2);
    EXPECT_EQ(boxesFilled(scene, ColorRole::Background).size(), 1);
    EXPECT_FALSE(itemsOf<SceneLine>(scene).isEmpty());
}

TEST(BitsLayoutTest, TheLayoutEndsAtTheLastDeclaredByteRoundedUpToAFullRow)
{
    EXPECT_EQ(rowCount(laidOut(QStringLiteral("bytes 4\nfield 0..5 x\n"), 360)), 2);
    EXPECT_EQ(rowCount(laidOut(QStringLiteral("bytes 4\nfield 0..3 x\n"), 360)), 1);
    EXPECT_EQ(rowCount(laidOut(QStringLiteral("bytes 4\nfield 11 x\n"), 360)), 3);
    EXPECT_EQ(rowCount(laidOut(QStringLiteral("bytes 16\nfield 16 x\n"), 360)), 2);
}

TEST(BitsLayoutTest, AFieldAcrossARowOfBytesIsSplitIntoOneBoxPerRowAndMarkedAsContinued)
{
    const Scene scene = laidOut(QStringLiteral("bytes 4\nfield 2..5 buf\nfield 0..1 a\n"
                                               "field 6..7 b\n"),
                                360);

    EXPECT_EQ(rowCount(scene), 2);
    EXPECT_EQ(boxesFilled(scene, ColorRole::BoxFill).size(), 4);
    EXPECT_EQ(dashedLines(scene), 2);
    const auto first = textLine(scene, QStringLiteral("buf"));
    const auto rest = textLine(scene, QStringLiteral("buf (cont.)"));
    ASSERT_TRUE(first && rest);
    EXPECT_LT(first->bounds.top(), rest->bounds.top());
    EXPECT_EQ(rest->color, ColorRole::MutedText);
    const QRectF firstBox = boxContaining(scene, first->bounds);
    const QRectF restBox = boxContaining(scene, rest->bounds);
    EXPECT_LT(restBox.left(), firstBox.left());
    EXPECT_GT(restBox.top(), firstBox.top());
}

TEST(BitsLayoutTest, ByteCellsNeverGrowBeyondTheirCapSoAWideScreenStaysCompact)
{
    const Scene scene = laidOut(QStringLiteral("bytes 4\nfield 0..3 x\n"), 1100);

    EXPECT_LT(scene.size.width(), 460);
    const QRectF box = boxContaining(scene, textLine(scene, QStringLiteral("x"))->bounds);
    EXPECT_LE(box.width(), 4 * 96 + kEpsilon);
}

TEST(BitsLayoutTest, TheTitleIsCentredAboveTheDiagram)
{
    const Scene scene = laidOut(kPacket, 360);

    const auto title = textLine(scene, QStringLiteral("struct Packet (12 B)"));
    ASSERT_TRUE(title);
    EXPECT_NEAR(title->bounds.center().x(), scene.size.width() / 2, kEpsilon);
    EXPECT_LE(title->bounds.top(), 4 + kEpsilon);
    EXPECT_LT(title->bounds.bottom(), boxesFilled(scene, ColorRole::BoxFill).first().rect.top());
}

TEST(BitsLayoutTest, TheSceneNeverExceedsTheWidthUnlessEvenTheMinimumIsWider)
{
    for (const qreal width : kWidths)
    {
        const Scene entry = laidOut(kEntry, width);
        const Scene packet = laidOut(kPacket, width);
        if (width >= 264)
            EXPECT_LE(entry.size.width(), width) << width;
        if (width >= 200)
            EXPECT_LE(packet.size.width(), width) << width;
    }
    EXPECT_GT(laidOut(kEntry, 100).size.width(), 100);
}

TEST(BitsLayoutTest, EveryPrimitiveStaysInsideTheSceneAndNoTwoTextsOverlap)
{
    for (const qreal width : kWidths)
    {
        expectInvariants(laidOut(kEntry, width), width);
        expectInvariants(laidOut(kPacket, width), width);
        expectInvariants(laidOut(kCrowded, width), width);
        expectInvariants(laidOut(QStringLiteral("bits 128\nfield 100..127 x\nfield 0 y\n"), width),
                         width);
        expectInvariants(
            laidOut(QStringLiteral("bits 1\nfield 0 a_flag_with_a_long_name\n"), width), width);
    }
}

TEST(BitsLayoutTest, ATextInsideABoxIsInsideItsBox)
{
    for (const qreal width : kWidths)
    {
        const Scene scene = laidOut(kCrowded, width);
        for (const SceneText& text : itemsOf<SceneText>(scene))
        {
            if (text.sizeFactor != 1 || text.align != Qt::AlignHCenter)
                continue;
            for (const SceneRect& rect : itemsOf<SceneRect>(scene))
            {
                if (rect.fill == ColorRole::BoxFill && overlaps(rect.rect, text.bounds))
                    EXPECT_TRUE(inside(rect.rect, text.bounds)) << width;
            }
        }
    }
}

TEST(BitsLayoutTest, TheCellsOfARegisterAreNeverNarrowerThanTheirNumbers)
{
    for (const qreal width : kWidths)
    {
        const Scene scene = laidOut(QStringLiteral("bits 128\nfield 0..127 v\n"), width);
        for (const SceneRect& rect : itemsOf<SceneRect>(scene))
            EXPECT_GE(rect.rect.width(), 3 * FakeTextMeasurer::kCharacterWidth * 0.8) << width;
    }
}

TEST(BitsLayoutTest, ThePaletteRolesAreTheSharedOnes)
{
    const Scene scene = laidOut(kEntry, 360);

    for (const SceneRect& rect : itemsOf<SceneRect>(scene))
    {
        EXPECT_TRUE(!rect.fill || rect.fill == ColorRole::BoxFill
                    || rect.fill == ColorRole::Background);
        EXPECT_TRUE(!rect.stroke || rect.stroke == ColorRole::BoxStroke
                    || rect.stroke == ColorRole::Divider);
    }
}

TEST(BitsLayoutTest, TheDiagramLayoutDispatchesABitsDiagram)
{
    const auto diagram = DiagramParser::parse(kEntry);
    ASSERT_TRUE(diagram.isSuccess()) << diagram.error().toStdString();
    const FakeTextMeasurer measurer;

    const Scene dispatched = DiagramLayout::layout(diagram.value(), DiagramStyle {}, 360, measurer);

    EXPECT_EQ(dispatched.size, laidOut(kEntry, 360).size);
}
