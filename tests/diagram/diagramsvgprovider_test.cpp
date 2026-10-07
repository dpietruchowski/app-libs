#include <QSignalSpy>
#include <QUrl>
#include <gtest/gtest.h>

#include "diagram/diagramsvgprovider.h"

using namespace diagram;

namespace
{
const QString kPrefix = QStringLiteral("data:image/svg+xml;utf8,");

QString decoded(const QString& source)
{
    return QUrl::fromPercentEncoding(source.mid(kPrefix.size()).toLatin1());
}

void configure(DiagramSvgProvider& provider, const QString& text, int width)
{
    provider.setAvailableWidth(width);
    provider.setText(text);
}
}

TEST(DiagramSvgProviderTest, TurnsTheTextIntoASvgDataUrl)
{
    DiagramSvgProvider provider;
    configure(provider, QStringLiteral("A -> B : hello & bye\n"), 360);

    ASSERT_TRUE(provider.svgSource().startsWith(kPrefix));
    const QString svg = decoded(provider.svgSource());
    EXPECT_TRUE(svg.startsWith(QStringLiteral("<svg ")));
    EXPECT_TRUE(svg.contains(QStringLiteral("hello &amp; bye")));
    EXPECT_FALSE(provider.svgSource().contains(QLatin1Char(' ')));
    EXPECT_TRUE(provider.error().isEmpty());
}

TEST(DiagramSvgProviderTest, ShowsTheImageAtItsLogicalSizeAndDrawsItLarger)
{
    DiagramSvgProvider provider;
    configure(provider, QStringLiteral("A -> B\n"), 360);

    EXPECT_EQ(provider.imageWidth(), 352);
    EXPECT_GT(provider.imageHeight(), 0);
    EXPECT_TRUE(decoded(provider.svgSource())
                    .contains(QStringLiteral("width=\"1056\" height=\"%1\" viewBox=\"0 0 352 %2\"")
                                  .arg(provider.imageHeight() * 3)
                                  .arg(provider.imageHeight())));
}

TEST(DiagramSvgProviderTest, LaysOutAtWholeStepsOfTheWidth)
{
    DiagramSvgProvider provider;
    configure(provider, QStringLiteral("A -> B\n"), 360);
    QSignalSpy changed(&provider, &DiagramSvgProvider::svgSourceChanged);

    provider.setAvailableWidth(365);
    EXPECT_EQ(changed.count(), 0);
    provider.setAvailableWidth(368);
    EXPECT_EQ(changed.count(), 1);
    EXPECT_EQ(provider.imageWidth(), 368);
}

TEST(DiagramSvgProviderTest, AnOverflowingSceneOnlyResizesWithinAStep)
{
    DiagramSvgProvider provider;
    configure(provider, QStringLiteral("A -> B\nB -> C\nC -> D\nD -> E\nE -> F\nF -> G\n"), 200);
    const QString source = provider.svgSource();
    QSignalSpy sourceChanged(&provider, &DiagramSvgProvider::svgSourceChanged);
    QSignalSpy sizeChanged(&provider, &DiagramSvgProvider::imageSizeChanged);

    for (const int width : { 199, 200, 201 })
    {
        provider.setAvailableWidth(width);
        EXPECT_EQ(provider.imageWidth(), width);
    }
    EXPECT_EQ(sourceChanged.count(), 0);
    EXPECT_EQ(sizeChanged.count(), 3);
    EXPECT_EQ(provider.svgSource(), source);
    EXPECT_TRUE(decoded(source).contains(QStringLiteral("viewBox=\"0 0 456 ")));
}

TEST(DiagramSvgProviderTest, KeepsTheProportionsWhenScalingDown)
{
    DiagramSvgProvider provider;
    configure(provider, QStringLiteral("A -> B\nB -> C\nC -> D\nD -> E\nE -> F\nF -> G\n"), 201);
    const QString svg = decoded(provider.svgSource());
    const qsizetype start = svg.indexOf(QStringLiteral("viewBox=\"0 0 456 ")) + 17;
    const qreal sceneHeight
        = svg.mid(start, svg.indexOf(QLatin1Char('"'), start) - start).toDouble();

    EXPECT_EQ(provider.imageHeight(), qRound(sceneHeight * 201 / 456));
}

TEST(DiagramSvgProviderTest, LimitsThePixelsOfATallDiagram)
{
    DiagramSvgProvider provider;
    configure(provider, QStringLiteral("A -> B : step\n").repeated(300), 720);
    const QString svg = decoded(provider.svgSource());
    const qsizetype start = svg.indexOf(QStringLiteral("width=\"")) + 7;
    const qreal naturalWidth
        = svg.mid(start, svg.indexOf(QLatin1Char('"'), start) - start).toDouble();
    const qreal ratio = naturalWidth / provider.imageWidth();

    EXPECT_LT(ratio, 3);
    EXPECT_GE(ratio, 1);
    EXPECT_LE(provider.imageWidth() * ratio * provider.imageHeight() * ratio,
              DiagramSvgProvider::kMaxImagePixels * 1.01);
}

TEST(DiagramSvgProviderTest, RejectsMeaninglessSettings)
{
    DiagramSvgProvider provider;
    configure(provider, QStringLiteral("A -> B\n"), 360);
    const QString source = provider.svgSource();

    provider.setFontPixelSize(0);
    provider.setFontFamily(QString());
    provider.setPixelRatio(0);
    EXPECT_EQ(provider.fontPixelSize(), DiagramStyle().fontPixelSize);
    EXPECT_DOUBLE_EQ(provider.pixelRatio(), 1);
    EXPECT_TRUE(decoded(provider.svgSource()).contains(QStringLiteral("width=\"352\"")));

    provider.setAvailableWidth(-5);
    EXPECT_EQ(provider.availableWidth(), 0);
    EXPECT_TRUE(provider.svgSource().isEmpty());
    provider.setAvailableWidth(360);
    EXPECT_FALSE(provider.svgSource().isEmpty());
}

TEST(DiagramSvgProviderTest, TheSameColoursOrUnknownRolesChangeNothing)
{
    DiagramSvgProvider provider;
    configure(provider, QStringLiteral("A -> B\n"), 360);
    const QVariantMap colors { { QStringLiteral("line"), QStringLiteral("#123456") } };
    provider.setColors(colors);
    QSignalSpy changed(&provider, &DiagramSvgProvider::svgSourceChanged);

    provider.setColors(colors);
    QVariantMap withExtra = colors;
    withExtra.insert(QStringLiteral("textPrimary"), QStringLiteral("#ff0000"));
    provider.setColors(withExtra);

    EXPECT_EQ(changed.count(), 0);
}

TEST(DiagramSvgProviderTest, ThePixelRatioOnlyChangesTheNaturalSize)
{
    DiagramSvgProvider provider;
    configure(provider, QStringLiteral("A -> B\n"), 360);
    const int height = provider.imageHeight();

    provider.setPixelRatio(2);

    EXPECT_TRUE(decoded(provider.svgSource()).contains(QStringLiteral("width=\"704\"")));
    EXPECT_EQ(provider.imageHeight(), height);
}

TEST(DiagramSvgProviderTest, ReportsAParseErrorAndShowsNothing)
{
    DiagramSvgProvider provider;
    configure(provider, QStringLiteral("A -> B\nend\n"), 360);

    EXPECT_EQ(provider.error(), QStringLiteral("Line 2: \"end\" without an open group"));
    EXPECT_TRUE(provider.svgSource().isEmpty());
    EXPECT_EQ(provider.imageWidth(), 0);

    provider.setText(QStringLiteral("A -> B\n"));
    EXPECT_TRUE(provider.error().isEmpty());
    EXPECT_FALSE(provider.svgSource().isEmpty());
}

TEST(DiagramSvgProviderTest, PassesTheWarningsOn)
{
    DiagramSvgProvider provider;
    configure(provider, QStringLiteral("autonumber\nA -> B\n"), 360);

    ASSERT_EQ(provider.warnings().size(), 1);
    EXPECT_TRUE(provider.warnings().first().startsWith(QStringLiteral("Line 1:")));
}

TEST(DiagramSvgProviderTest, ColoursChangeOnlyTheColours)
{
    DiagramSvgProvider provider;
    configure(provider, QStringLiteral("A -> B : x\n"), 360);
    const int height = provider.imageHeight();

    provider.setColors({ { QStringLiteral("background"), QColor(0x10, 0x20, 0x30) },
                         { QStringLiteral("text"), QStringLiteral("#eeeeee") } });

    const QString svg = decoded(provider.svgSource());
    EXPECT_TRUE(svg.contains(QStringLiteral("fill=\"#102030\"")));
    EXPECT_TRUE(svg.contains(QStringLiteral("fill=\"#eeeeee\">x</text>")));
    EXPECT_EQ(provider.imageHeight(), height);
}

TEST(DiagramSvgProviderTest, ShowsNothingWithoutTextOrWidth)
{
    DiagramSvgProvider provider;
    provider.setText(QStringLiteral("A -> B\n"));
    EXPECT_TRUE(provider.svgSource().isEmpty());

    provider.setAvailableWidth(360);
    EXPECT_FALSE(provider.svgSource().isEmpty());

    provider.setText(QStringLiteral("   "));
    EXPECT_TRUE(provider.svgSource().isEmpty());
    EXPECT_TRUE(provider.error().isEmpty());
}
