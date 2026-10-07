#include <gtest/gtest.h>

#include "diagram/sequencelayout.h"
#include "diagram/sequenceparser.h"
#include "faketextmeasurer.h"

using namespace diagram;

namespace
{
constexpr qreal kEpsilon = 0.01;

const QString kRichDiagram
    = QStringLiteral("title Signing in with a password and a second factor\n"
                     "actor User\n"
                     "participant \"Web app\" as Web\n"
                     "participant \"Authorization server\" as Auth\n"
                     "database Accounts\n"
                     "User -> Web : types login and password\n"
                     "Web -> Auth : POST /token with the credentials\n"
                     "Auth -> Accounts : look up the account by login\n"
                     "Accounts --> Auth : password hash\n"
                     "Auth -> Auth : compare hashes in constant time\n"
                     "note right of Accounts : stored with a per-user salt\n"
                     "alt the password matches\n"
                     "  Auth -> User : ask for the second factor\n"
                     "  loop until the code is valid\n"
                     "    User -> Auth : one-time code\n"
                     "  end\n"
                     "  Auth --> Web : access token\n"
                     "else it does not\n"
                     "  Auth --> Web : 401\n"
                     "end\n"
                     "== Later ==\n"
                     "note over User, Auth : the token expires after an hour\n"
                     "Web -> Web : refresh the token\n");

SequenceDiagram parsed(const QString& text)
{
    const auto result = SequenceParser::parse(text);
    EXPECT_TRUE(result.isSuccess());
    return result.isSuccess() ? result.value() : SequenceDiagram {};
}

Scene laidOut(const QString& text, qreal width)
{
    static const FakeTextMeasurer measurer;
    return SequenceLayout::layout(parsed(text), DiagramStyle {}, width, measurer);
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

bool inside(const QRectF& outer, const QRectF& inner)
{
    return inner.left() >= outer.left() - kEpsilon && inner.top() >= outer.top() - kEpsilon
        && inner.right() <= outer.right() + kEpsilon && inner.bottom() <= outer.bottom() + kEpsilon;
}

QList<qreal> lifelineXs(const Scene& scene, qsizetype count)
{
    QList<qreal> xs;
    for (const SceneLine& line : itemsOf<SceneLine>(scene).mid(0, count))
        xs.append(line.points.first().x());
    return xs;
}

bool isLifelineX(const QList<qreal>& xs, qreal x)
{
    return std::any_of(xs.cbegin(), xs.cend(),
                       [x](qreal lifeline) { return std::abs(lifeline - x) < kEpsilon; });
}

QList<SceneLine> messageLines(const Scene& scene)
{
    QList<SceneLine> found;
    for (const SceneLine& line : itemsOf<SceneLine>(scene))
    {
        if (line.head != ArrowHead::None)
            found.append(line);
    }
    return found;
}

SceneText textStarting(const Scene& scene, const QString& start)
{
    for (const SceneText& text : itemsOf<SceneText>(scene))
    {
        if (text.lines.first().startsWith(start))
            return text;
    }
    ADD_FAILURE() << "no text starts with " << start.toStdString();
    return SceneText {};
}

bool madeOfWholeWords(const SceneText& text, const QString& label)
{
    const QStringList words = label.split(QLatin1Char(' '));
    return std::all_of(text.lines.cbegin(), text.lines.cend(),
                       [&words](const QString& line)
                       {
                           const QStringList parts = line.split(QLatin1Char(' '));
                           return std::all_of(parts.cbegin(), parts.cend(),
                                              [&words](const QString& part)
                                              { return words.contains(part); });
                       });
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
    for (const SceneLine& line : itemsOf<SceneLine>(scene))
    {
        for (const QPointF& point : line.points)
            EXPECT_TRUE(inside(area, QRectF(point, QSizeF(0, 0)))) << availableWidth;
    }

    const QList<SceneText> texts = itemsOf<SceneText>(scene);
    for (qsizetype i = 0; i < texts.size(); ++i)
    {
        EXPECT_TRUE(inside(area, texts[i].bounds)) << availableWidth << " " << i;
        for (const QString& line : texts[i].lines)
            EXPECT_LE(FakeTextMeasurer().width(line), texts[i].bounds.width() + kEpsilon);
        for (qsizetype j = i + 1; j < texts.size(); ++j)
        {
            const QRectF overlap = texts[i].bounds.intersected(texts[j].bounds);
            EXPECT_TRUE(overlap.width() < kEpsilon || overlap.height() < kEpsilon)
                << availableWidth << " texts " << i << " and " << j
                << " overlap: " << texts[i].lines.join(QLatin1Char(' ')).toStdString() << " / "
                << texts[j].lines.join(QLatin1Char(' ')).toStdString();
        }
    }
}
}

TEST(SequenceLayoutTest, KeepsTheInvariantsAtPhoneAndDesktopWidths)
{
    for (const qreal width : { 280.0, 360.0, 720.0 })
    {
        const Scene scene = laidOut(kRichDiagram, width);
        EXPECT_GE(width / scene.size.width(), 0.75);
        expectInvariants(scene, width);
    }
}

TEST(SequenceLayoutTest, FillsTheWidthWhenTheParticipantsFit)
{
    for (const qreal width : { 360.0, 720.0 })
        EXPECT_DOUBLE_EQ(laidOut(kRichDiagram, width).size.width(), width);
}

TEST(SequenceLayoutTest, MessagesStartAndEndOnLifelines)
{
    const Scene scene = laidOut(kRichDiagram, 360);
    const QList<qreal> xs = lifelineXs(scene, 4);

    for (const SceneLine& line : messageLines(scene))
    {
        EXPECT_TRUE(isLifelineX(xs, line.points.first().x()));
        EXPECT_TRUE(isLifelineX(xs, line.points.last().x()));
    }
}

TEST(SequenceLayoutTest, MessagesGoDownTheDiagram)
{
    const QList<SceneLine> lines = messageLines(laidOut(kRichDiagram, 360));

    ASSERT_EQ(lines.size(), 10);
    for (qsizetype index = 1; index < lines.size(); ++index)
        EXPECT_GT(lines[index].points.first().y(), lines[index - 1].points.last().y());
}

TEST(SequenceLayoutTest, LifelinesDivideTheWidthEvenly)
{
    const Scene scene = laidOut(QStringLiteral("A -> B\nB -> C\n"), 300);
    const DiagramStyle style;
    const qreal column = (300 - 2 * style.margin) / 3;

    EXPECT_EQ(lifelineXs(scene, 3),
              (QList<qreal> { style.margin + column / 2, style.margin + column * 1.5,
                              style.margin + column * 2.5 }));
    EXPECT_DOUBLE_EQ(scene.size.width(), 300);
}

TEST(SequenceLayoutTest, ANarrowerWidthWrapsInsteadOfShrinking)
{
    const Scene wide = laidOut(kRichDiagram, 720);
    const Scene narrow = laidOut(kRichDiagram, 280);

    EXPECT_GT(textLineCount(narrow), textLineCount(wide));
    EXPECT_GT(narrow.size.height(), wide.size.height());
    EXPECT_DOUBLE_EQ(narrow.fontPixelSize, wide.fontPixelSize);
}

TEST(SequenceLayoutTest, BreaksATokenLongerThanItsColumn)
{
    const Scene scene
        = laidOut(QStringLiteral("participant OAuthAuthorizationServerWithAVeryLongName\n"
                                 "participant B\nparticipant C\nparticipant D\n"),
                  280);

    expectInvariants(scene, 280);
    EXPECT_GT(itemsOf<SceneText>(scene).first().lines.size(), 1);
}

TEST(SequenceLayoutTest, TooManyParticipantsOverflowAtTheMinimumColumnWidth)
{
    const Scene scene = laidOut(QStringLiteral("A -> B\nB -> C\nC -> D\nD -> E\nE -> F\nF -> G\n"
                                               "G -> H\n"),
                                360);
    const DiagramStyle style;

    EXPECT_DOUBLE_EQ(scene.size.width(), 2 * style.margin + 8 * style.minColumnWidth);
    expectInvariants(scene, 360);
}

TEST(SequenceLayoutTest, ASelfMessageOnTheLastColumnTurnsLeft)
{
    const Scene scene = laidOut(QStringLiteral("A -> B\nB -> C\nC -> C : think about it\n"), 360);
    const QList<qreal> xs = lifelineXs(scene, 3);
    const SceneText label = textStarting(scene, QStringLiteral("think"));

    EXPECT_DOUBLE_EQ(scene.size.width(), 360);
    expectInvariants(scene, 360);
    EXPECT_LT(label.bounds.right(), xs[2]);
    EXPECT_EQ(label.lines, QStringList { QStringLiteral("think about it") });
    EXPECT_LT(messageLines(scene).last().points[1].x(), xs[2]);
}

TEST(SequenceLayoutTest, ASelfMessageLabelCrossesLifelinesOnTheRoomierSide)
{
    const Scene scene = laidOut(QStringLiteral("A -> B\nB -> B : compare hashes in constant time\n"
                                               "B -> C\nC -> D\nD -> E\n"),
                                360);
    const QList<qreal> xs = lifelineXs(scene, 5);
    const SceneText label = textStarting(scene, QStringLiteral("compare"));

    EXPECT_GT(label.bounds.left(), xs[1]);
    EXPECT_GT(label.bounds.right(), xs[2]);
    EXPECT_TRUE(madeOfWholeWords(label, QStringLiteral("compare hashes in constant time")));
}

TEST(SequenceLayoutTest, MessageLabelsWrapAtWordsAndMayOutgrowTheirArrow)
{
    const Scene scene = laidOut(kRichDiagram, 360);
    const SceneText label = textStarting(scene, QStringLiteral("POST"));
    const QList<qreal> xs = lifelineXs(scene, 4);

    EXPECT_TRUE(madeOfWholeWords(label, QStringLiteral("POST /token with the credentials")));
    EXPECT_GT(label.bounds.width(), xs[2] - xs[1]);
    EXPECT_NEAR(label.bounds.center().x(), (xs[1] + xs[2]) / 2, kEpsilon);
}

TEST(SequenceLayoutTest, AWideLabelNearTheEdgeIsShiftedIntoTheScene)
{
    const Scene scene
        = laidOut(QStringLiteral("participant A\nparticipant B\nparticipant C\nparticipant D\n"
                                 "C -> D : a label that is much wider than one column\n"),
                  360);

    expectInvariants(scene, 360);
    EXPECT_LE(textStarting(scene, QStringLiteral("a label")).bounds.right(),
              scene.size.width() - DiagramStyle().margin + kEpsilon);
}

TEST(SequenceLayoutTest, ColumnsGrowForLongParticipantNames)
{
    const Scene scene = laidOut(kRichDiagram, 360);
    const QList<qreal> xs = lifelineXs(scene, 4);

    EXPECT_GT(xs[2] - xs[1], xs[1] - xs[0]);
    EXPECT_TRUE(madeOfWholeWords(textStarting(scene, QStringLiteral("Authorization")),
                                 QStringLiteral("Authorization server")));
    EXPECT_DOUBLE_EQ(scene.size.width(), 360);
}

TEST(SequenceLayoutTest, ASideNoteSitsBesideItsLifeline)
{
    const Scene scene = laidOut(QStringLiteral("A -> B\nB -> C\nnote right of B : kept\n"
                                               "note left of B : here\n"),
                                360);
    const QList<qreal> xs = lifelineXs(scene, 3);

    EXPECT_GT(textStarting(scene, QStringLiteral("kept")).bounds.left(), xs[1]);
    EXPECT_LT(textStarting(scene, QStringLiteral("here")).bounds.right(), xs[1]);
}

TEST(SequenceLayoutTest, ASideNoteWithoutRoomIsCentredOverItsLifeline)
{
    const Scene scene = laidOut(QStringLiteral("A -> B\nnote right of B : x\n"), 160);
    const QList<qreal> xs = lifelineXs(scene, 2);

    expectInvariants(scene, 160);
    EXPECT_NEAR(textStarting(scene, QStringLiteral("x")).bounds.center().x(), xs[1], kEpsilon);
}

TEST(SequenceLayoutTest, ANoteThatWouldWrapInANarrowSideGoesOver)
{
    const Scene scene = laidOut(
        QStringLiteral("A -> B\nB -> C\nC -> D\nnote right of D : stored with a per-user salt\n"),
        720);

    EXPECT_EQ(textStarting(scene, QStringLiteral("stored")).lines.size(), 1);
}

TEST(SequenceLayoutTest, SideNotesTouchTheGapBesideTheirLifeline)
{
    const Scene scene = laidOut(QStringLiteral("A -> B\nB -> C\nnote left of B : here\n"
                                               "note right of B : kept\n"),
                                360);
    const QList<qreal> xs = lifelineXs(scene, 3);
    const qreal halfGap = DiagramStyle().columnGap / 2;
    QList<QRectF> notes;
    for (const SceneRect& rect : itemsOf<SceneRect>(scene))
    {
        if (rect.fill == ColorRole::NoteFill)
            notes.append(rect.rect);
    }

    ASSERT_EQ(notes.size(), 2);
    EXPECT_NEAR(notes[0].right(), xs[1] - halfGap, kEpsilon);
    EXPECT_NEAR(notes[1].left(), xs[1] + halfGap, kEpsilon);
}

TEST(SequenceLayoutTest, ANoteFallingBackToOverIsCentredOnItsLifeline)
{
    const Scene scene = laidOut(
        QStringLiteral("A -> B\nB -> C\nC -> D\nnote right of D : stored with a salt\n"), 720);
    const QList<qreal> xs = lifelineXs(scene, 4);

    EXPECT_NEAR(textStarting(scene, QStringLiteral("stored")).bounds.center().x(), xs[3], kEpsilon);
}

TEST(SequenceLayoutTest, ANarrowSceneKeepsDeepGroupsInside)
{
    QString text = QStringLiteral("A -> B\n");
    for (int index = 0; index < 8; ++index)
        text += QStringLiteral("critical when the remote service answers slowly\n");
    text += QStringLiteral("A -> B : x\n");
    for (int index = 0; index < 8; ++index)
        text += QStringLiteral("end\n");

    expectInvariants(laidOut(text, 120), 120);
}

TEST(SequenceLayoutTest, TheSceneHasAWholePixelSize)
{
    for (const qreal width : { 199.5, 280.0, 361.3 })
    {
        const QSizeF size = laidOut(kRichDiagram, width).size;
        EXPECT_DOUBLE_EQ(size.width(), std::ceil(size.width()));
        EXPECT_DOUBLE_EQ(size.height(), std::ceil(size.height()));
    }
}

TEST(SequenceLayoutTest, TheFootHangsFromTheLifelines)
{
    QString text = QStringLiteral("participant \"Two\\nlines\" as A\nparticipant B\n");
    for (int index = 0; index < 9; ++index)
        text += QStringLiteral("A -> B\n");
    const Scene scene = laidOut(text, 360);
    QList<qreal> footTops;
    for (const SceneRect& rect : itemsOf<SceneRect>(scene))
    {
        if (rect.fill == ColorRole::BoxFill && rect.rect.top() > scene.size.height() / 2)
            footTops.append(rect.rect.top());
    }

    ASSERT_EQ(footTops.size(), 2);
    EXPECT_DOUBLE_EQ(footTops[0], footTops[1]);
    EXPECT_DOUBLE_EQ(footTops[0], itemsOf<SceneLine>(scene).first().points.last().y());
}

TEST(SequenceLayoutTest, DeepNestingStopsIndenting)
{
    QString text = QStringLiteral("A -> B\n");
    for (int index = 0; index < 40; ++index)
        text += QStringLiteral("alt level\n");
    text += QStringLiteral("A -> B : deep\n");
    for (int index = 0; index < 40; ++index)
        text += QStringLiteral("end\n");

    expectInvariants(laidOut(text, 280), 280);
}

TEST(SequenceLayoutTest, BlankLabelsDrawNothing)
{
    const Scene scene = laidOut(QStringLiteral("A -> B :    \nnote over A :   \n"), 360);

    for (const SceneText& text : itemsOf<SceneText>(scene))
        EXPECT_NE(text.lines.join(QString()).trimmed(), QString());
}

TEST(SequenceLayoutTest, StepsFollowEachOtherDownTheDiagram)
{
    const Scene scene = laidOut(kRichDiagram, 360);
    const QStringList order { QStringLiteral("types"),     QStringLiteral("POST"),
                              QStringLiteral("look"),      QStringLiteral("password hash"),
                              QStringLiteral("compare"),   QStringLiteral("stored"),
                              QStringLiteral("[the"),      QStringLiteral("ask"),
                              QStringLiteral("[until"),    QStringLiteral("one-time"),
                              QStringLiteral("access"),    QStringLiteral("[it does"),
                              QStringLiteral("401"),       QStringLiteral("Later"),
                              QStringLiteral("the token"), QStringLiteral("refresh") };

    for (qsizetype index = 1; index < order.size(); ++index)
        EXPECT_GT(textStarting(scene, order[index]).bounds.top(),
                  textStarting(scene, order[index - 1]).bounds.top())
            << order[index].toStdString();
}

TEST(SequenceLayoutTest, KeepsTheInvariantsWhenTheSceneOverflows)
{
    for (const qreal width : { 120.0, 200.0 })
        expectInvariants(laidOut(kRichDiagram, width), width);
}

TEST(SequenceLayoutTest, ALabelGetsABackgroundRightBeforeItsText)
{
    const Scene scene = laidOut(QStringLiteral("A -> C : crosses B\nparticipant B\n"), 360);

    qsizetype labelIndex = -1;
    for (qsizetype index = 0; index < scene.items.size(); ++index)
    {
        const auto* text = std::get_if<SceneText>(&scene.items[index]);
        if (text && text->lines == QStringList { QStringLiteral("crosses B") })
            labelIndex = index;
    }
    ASSERT_GT(labelIndex, 0);
    const auto* background = std::get_if<SceneRect>(&scene.items[labelIndex - 1]);
    ASSERT_NE(background, nullptr);
    EXPECT_EQ(background->fill, ColorRole::Background);
    const qreal halo = DiagramStyle().labelHalo;
    EXPECT_EQ(background->rect,
              std::get<SceneText>(scene.items[labelIndex]).bounds.adjusted(-halo, 0, halo, 0));
    const SceneLine arrow = messageLines(scene).first();
    EXPECT_LT(background->rect.bottom(),
              arrow.points.first().y() - DiagramStyle().arrowHeadSize * 0.45);
}

TEST(SequenceLayoutTest, GroupFramesNestAndSpanTheScene)
{
    const Scene scene = laidOut(kRichDiagram, 360);
    QList<SceneRect> frames;
    for (const SceneRect& rect : itemsOf<SceneRect>(scene))
    {
        if (!rect.fill && rect.stroke == ColorRole::GroupStroke)
            frames.append(rect);
    }

    ASSERT_EQ(frames.size(), 2);
    const QRectF inner = frames[0].rect;
    const QRectF outer = frames[1].rect;
    EXPECT_TRUE(inside(outer, inner));
    EXPECT_LT(inner.width(), outer.width());
    EXPECT_DOUBLE_EQ(outer.left(), DiagramStyle().margin);
    EXPECT_DOUBLE_EQ(outer.right(), scene.size.width() - DiagramStyle().margin);
}

TEST(SequenceLayoutTest, FootRepeatsOnlyOnLongDiagrams)
{
    const auto boxCount = [](const Scene& scene)
    {
        qsizetype count = 0;
        for (const SceneRect& rect : itemsOf<SceneRect>(scene))
            count += rect.fill == ColorRole::BoxFill && rect.stroke == ColorRole::BoxStroke;
        return count;
    };

    EXPECT_EQ(boxCount(laidOut(QStringLiteral("A -> B\n"), 360)), 2);
    EXPECT_EQ(boxCount(laidOut(QStringLiteral("A -> B\nB -> A\nA -> B\nB -> A\nA -> B\n"
                                              "B -> A\nA -> B\nB -> A\nA -> B\n"),
                               360)),
              4);
}

TEST(SequenceLayoutTest, AnEmptyDelayStillTakesSpace)
{
    const Scene without = laidOut(QStringLiteral("A -> B\nA -> B\n"), 360);
    const Scene with = laidOut(QStringLiteral("A -> B\n...\nA -> B\n"), 360);

    EXPECT_GT(with.size.height(), without.size.height());
}

TEST(SequenceLayoutTest, IsDeterministic)
{
    const Scene first = laidOut(kRichDiagram, 360);
    const Scene second = laidOut(kRichDiagram, 360);

    ASSERT_EQ(first.items.size(), second.items.size());
    EXPECT_EQ(first.size, second.size);
}

TEST(SequenceLayoutTest, CarriesTheFontForTheWriter)
{
    const Scene scene = laidOut(QStringLiteral("A -> B\n"), 360);

    EXPECT_EQ(scene.fontFamily, DiagramStyle().fontFamily);
    EXPECT_DOUBLE_EQ(scene.lineHeight, FakeTextMeasurer::kLineHeight);
    EXPECT_DOUBLE_EQ(scene.ascent, FakeTextMeasurer::kAscent);
}
