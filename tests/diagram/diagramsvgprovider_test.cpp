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

TEST(DiagramSvgProviderTest, DrawsABlockDiagram)
{
    DiagramSvgProvider provider;
    configure(provider,
              QStringLiteral("@startuml\nrectangle \"SoC\" {\nrectangle CPU\nrectangle ROM\n}\n"
                             "rectangle DDR\nCPU -- ROM\nCPU -- DDR\n@enduml\n"),
              360);

    EXPECT_TRUE(provider.error().isEmpty());
    ASSERT_FALSE(provider.svgSource().isEmpty());
    const QString svg = decoded(provider.svgSource());
    EXPECT_TRUE(svg.contains(QStringLiteral(">CPU</text>")));
    EXPECT_TRUE(svg.contains(QStringLiteral(">SoC</text>")));
    EXPECT_GT(provider.imageHeight(), 0);
}

TEST(DiagramSvgProviderTest, FillWidthCentersANarrowDiagramOnAFullWidthCanvas)
{
    DiagramSvgProvider provider;
    configure(provider, QStringLiteral("rectangle CPU\nrectangle RAM\nCPU -- RAM\n"), 336);
    const int narrowWidth = provider.imageWidth();
    const int height = provider.imageHeight();
    ASSERT_LT(narrowWidth, 336);

    provider.setFillWidth(true);

    EXPECT_EQ(provider.imageWidth(), 336);
    EXPECT_EQ(provider.imageHeight(), height);
    const QString svg = decoded(provider.svgSource());
    EXPECT_TRUE(svg.contains(QStringLiteral("viewBox=\"0 0 336 %1\"").arg(height)));
    const int margin = (336 - narrowWidth) / 2;
    EXPECT_TRUE(svg.contains(QStringLiteral("<rect x=\"%1\"").arg(margin + 4)));

    provider.setFillWidth(false);
    EXPECT_EQ(provider.imageWidth(), narrowWidth);
}

TEST(DiagramSvgProviderTest, FillWidthFollowsTheWidthWithinAStep)
{
    DiagramSvgProvider provider;
    provider.setFillWidth(true);
    configure(provider, QStringLiteral("rectangle CPU\n"), 336);
    QSignalSpy changed(&provider, &DiagramSvgProvider::svgSourceChanged);

    provider.setAvailableWidth(340);

    EXPECT_EQ(changed.count(), 1);
    EXPECT_EQ(provider.imageWidth(), 340);
}

TEST(DiagramSvgProviderTest, ReportsABlockError)
{
    DiagramSvgProvider provider;
    configure(provider, QStringLiteral("rectangle A\nA -- B\n"), 360);

    EXPECT_TRUE(provider.error().startsWith(QStringLiteral("Line 2: unknown block \"B\"")));
    EXPECT_TRUE(provider.svgSource().isEmpty());
}

TEST(DiagramSvgProviderTest, DrawsAStateDiagram)
{
    DiagramSvgProvider provider;
    configure(provider,
              QStringLiteral("@startuml\n[*] --> Ready\nReady --> Running : scheduled\n"
                             "Running --> Ready : preempted\nRunning --> [*] : exit\n@enduml\n"),
              336);

    EXPECT_TRUE(provider.error().isEmpty());
    ASSERT_FALSE(provider.svgSource().isEmpty());
    const QString svg = decoded(provider.svgSource());
    EXPECT_TRUE(svg.contains(QStringLiteral(">Ready</text>")));
    EXPECT_TRUE(svg.contains(QStringLiteral(">scheduled</text>")));
    EXPECT_TRUE(svg.contains(QStringLiteral("<ellipse")));
    EXPECT_GT(provider.imageHeight(), 0);
    EXPECT_LE(provider.imageWidth(), 336);
}

TEST(DiagramSvgProviderTest, ReportsAStateError)
{
    DiagramSvgProvider provider;
    configure(provider, QStringLiteral("[*] --> A\nA --> [H]\n"), 336);

    EXPECT_TRUE(provider.error().startsWith(
        QStringLiteral("Line 2: \"A --> [H]\" is valid PlantUML, but this app does not support "
                       "history states")));
    EXPECT_TRUE(provider.svgSource().isEmpty());
}

TEST(DiagramSvgProviderTest, DrawsAClassDiagram)
{
    DiagramSvgProvider provider;
    configure(provider,
              QStringLiteral("@startuml\nstruct task_struct {\n  pid : int\n}\n"
                             "struct mm_struct\n"
                             "task_struct \"1\" --> \"0..1\" mm_struct : mm\n@enduml\n"),
              336);

    EXPECT_TRUE(provider.error().isEmpty());
    ASSERT_FALSE(provider.svgSource().isEmpty());
    const QString svg = decoded(provider.svgSource());
    EXPECT_TRUE(svg.contains(QStringLiteral(">task_struct</text>")));
    EXPECT_TRUE(svg.contains(QStringLiteral(">pid : int</text>")));
    EXPECT_TRUE(svg.contains(QStringLiteral(">0..1</text>")));
    EXPECT_GT(provider.imageHeight(), 0);
    EXPECT_LE(provider.imageWidth(), 336);
}

TEST(DiagramSvgProviderTest, ReportsAClassError)
{
    DiagramSvgProvider provider;
    configure(provider, QStringLiteral("class A\npackage p {\n}\n"), 336);

    EXPECT_TRUE(provider.error().startsWith(
        QStringLiteral("Line 2: \"package p {\" is valid PlantUML, but this app does not support "
                       "\"package\" and \"namespace\" blocks")));
    EXPECT_TRUE(provider.svgSource().isEmpty());
}

TEST(DiagramSvgProviderTest, DrawsAnActivityDiagram)
{
    DiagramSvgProvider provider;
    configure(provider,
              QStringLiteral("@startuml\nstart\n:Do it;\nif (ok?) then (yes)\n:Next;\nelse (no)\n"
                             "stop\nendif\nstop\n@enduml\n"),
              360);

    EXPECT_TRUE(provider.error().isEmpty());
    ASSERT_FALSE(provider.svgSource().isEmpty());
    const QString svg = decoded(provider.svgSource());
    EXPECT_TRUE(svg.contains(QStringLiteral("<ellipse")));
    EXPECT_TRUE(svg.contains(QStringLiteral("<polygon")));
    EXPECT_TRUE(svg.contains(QStringLiteral(">Do it</text>")));
    EXPECT_GT(provider.imageHeight(), 0);
}

TEST(DiagramSvgProviderTest, ReportsAnActivityErrorAndItsWarnings)
{
    DiagramSvgProvider provider;
    configure(provider, QStringLiteral("skinparam monochrome true\nstart\n|Lane|\n"), 360);

    EXPECT_TRUE(
        provider.error().startsWith(QStringLiteral("Line 3: \"|Lane|\" is valid PlantUML")));
    EXPECT_TRUE(provider.svgSource().isEmpty());

    provider.setText(QStringLiteral("skinparam monochrome true\nstart\nstop\n"));
    EXPECT_TRUE(provider.error().isEmpty());
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
