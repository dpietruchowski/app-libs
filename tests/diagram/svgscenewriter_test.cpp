#include <QSet>
#include <QXmlStreamReader>
#include <gtest/gtest.h>

#include "diagram/sequencelayout.h"
#include "diagram/sequenceparser.h"
#include "diagram/svgscenewriter.h"
#include "faketextmeasurer.h"

using namespace diagram;

namespace
{
Scene smallScene()
{
    Scene scene;
    scene.size = QSizeF(100, 50);
    scene.fontFamily = QStringLiteral("sans-serif");
    scene.fontPixelSize = 14;
    scene.lineHeight = 16;
    scene.ascent = 12;
    scene.arrowHeadSize = 8;
    scene.items = {
        SceneLine { { QPointF(20, 0), QPointF(20, 50) }, ColorRole::Line, true, ArrowHead::None },
        SceneRect { QRectF(0, 0, 40, 20), 4, ColorRole::BoxFill, ColorRole::BoxStroke, false },
        SceneLine {
            { QPointF(20, 30), QPointF(80, 30) }, ColorRole::Line, false, ArrowHead::Filled },
        SceneText { QRectF(10, 2, 60, 32),
                    Qt::AlignLeft,
                    { QStringLiteral("a < b"), QStringLiteral("\"x\" & 'y'") },
                    ColorRole::Text },
    };
    return scene;
}

struct XmlSummary
{
    bool wellFormed = false;
    QSet<QString> elements;
};

XmlSummary summarize(const QString& svg)
{
    XmlSummary summary;
    QXmlStreamReader reader(svg);
    while (!reader.atEnd())
    {
        if (reader.readNext() == QXmlStreamReader::StartElement)
            summary.elements.insert(reader.name().toString());
    }
    summary.wellFormed = !reader.hasError();
    return summary;
}
}

TEST(SvgSceneWriterTest, WritesTheExactDocumentForASmallScene)
{
    const QString expected = QStringLiteral(
        "<svg xmlns=\"http://www.w3.org/2000/svg\" version=\"1.2\" baseProfile=\"tiny\" "
        "width=\"100\" height=\"50\" viewBox=\"0 0 100 50\">\n"
        "<rect x=\"0\" y=\"0\" width=\"100\" height=\"50\" fill=\"#ffffff\"/>\n"
        "<g font-family=\"sans-serif\" font-size=\"14\" stroke-width=\"1\">\n"
        "<line x1=\"20\" y1=\"0\" x2=\"20\" y2=\"50\" fill=\"none\" stroke=\"#444444\" "
        "stroke-dasharray=\"4 3\"/>\n"
        "<rect x=\"0\" y=\"0\" width=\"40\" height=\"20\" rx=\"4\" ry=\"4\" fill=\"#eef0f8\" "
        "stroke=\"#555f7a\"/>\n"
        "<line x1=\"20\" y1=\"30\" x2=\"80\" y2=\"30\" fill=\"none\" stroke=\"#444444\"/>\n"
        "<polygon points=\"72,33.6 80,30 72,26.4\" fill=\"#444444\"/>\n"
        "<text x=\"10\" y=\"14\" text-anchor=\"start\" fill=\"#222222\">a &lt; b</text>\n"
        "<text x=\"10\" y=\"30\" text-anchor=\"start\" fill=\"#222222\">"
        "&quot;x&quot; &amp; &apos;y&apos;</text>\n"
        "</g>\n"
        "</svg>\n");

    EXPECT_EQ(SvgSceneWriter::write(smallScene(), DiagramPalette {}), expected);
}

TEST(SvgSceneWriterTest, DeclaresThePhysicalSizeAndKeepsTheLogicalViewBox)
{
    const QString svg = SvgSceneWriter::write(smallScene(), DiagramPalette {}, 3);

    EXPECT_TRUE(
        svg.contains(QStringLiteral("width=\"300\" height=\"150\" viewBox=\"0 0 100 50\"")));
}

TEST(SvgSceneWriterTest, AnOpenHeadIsAPolylineAndCentredTextIsAnchoredInTheMiddle)
{
    Scene scene = smallScene();
    scene.items = {
        SceneLine { { QPointF(80, 30), QPointF(20, 30) }, ColorRole::Line, false, ArrowHead::Open },
        SceneText { QRectF(10, 0, 60, 16),
                    Qt::AlignHCenter,
                    { QStringLiteral("mid") },
                    ColorRole::MutedText },
    };
    const QString svg = SvgSceneWriter::write(scene, DiagramPalette {});

    EXPECT_TRUE(svg.contains(QStringLiteral("<polyline points=\"28,26.4 20,30 28,33.6\" "
                                            "fill=\"none\" stroke=\"#444444\"/>")));
    EXPECT_TRUE(svg.contains(QStringLiteral("x=\"40\" y=\"12\" text-anchor=\"middle\" "
                                            "fill=\"#666666\">mid</text>")));
}

TEST(SvgSceneWriterTest, AHeadOnALoopPointsAlongItsLastSegment)
{
    Scene scene = smallScene();
    scene.items = {
        SceneLine { { QPointF(50, 10), QPointF(26, 10), QPointF(26, 30), QPointF(50, 30) },
                    ColorRole::Line,
                    false,
                    ArrowHead::Filled },
    };
    const QString svg = SvgSceneWriter::write(scene, DiagramPalette {});

    EXPECT_TRUE(svg.contains(QStringLiteral("<polygon points=\"42,33.6 50,30 42,26.4\"")));
}

TEST(SvgSceneWriterTest, DropsCharactersThatXmlForbids)
{
    Scene scene = smallScene();
    scene.items = { SceneText { QRectF(0, 0, 60, 16),
                                Qt::AlignLeft,
                                { QStringLiteral("a\x0c"
                                                 "b\x01"
                                                 "c")
                                  + QChar(0xFFFF) },
                                ColorRole::Text } };
    const QString svg = SvgSceneWriter::write(scene, DiagramPalette {});

    EXPECT_TRUE(summarize(svg).wellFormed);
    EXPECT_TRUE(svg.contains(QStringLiteral(">abc</text>")));
}

TEST(SvgSceneWriterTest, KeepsTheAlphaOfTranslucentColours)
{
    DiagramPalette palette;
    palette.background = Qt::transparent;
    palette.boxFill = QColor(0x10, 0x20, 0x30, 0x80);
    const QString svg = SvgSceneWriter::write(smallScene(), palette);

    EXPECT_TRUE(
        svg.contains(QStringLiteral("height=\"50\" fill=\"#000000\" fill-opacity=\"0\"/>")));
    EXPECT_TRUE(svg.contains(QStringLiteral("fill=\"#102030\" fill-opacity=\"0.5\"")));
    EXPECT_FALSE(svg.contains(QStringLiteral("stroke-opacity")));
}

TEST(SvgSceneWriterTest, ThePaletteOnlyChangesColours)
{
    DiagramPalette night;
    night.background = QColor(0x10, 0x10, 0x10);
    night.text = QColor(0xee, 0xee, 0xee);

    QString light = SvgSceneWriter::write(smallScene(), DiagramPalette {});
    const QString dark = SvgSceneWriter::write(smallScene(), night);

    EXPECT_NE(light, dark);
    light.replace(QStringLiteral("#ffffff"), QStringLiteral("#101010"));
    light.replace(QStringLiteral("#222222"), QStringLiteral("#eeeeee"));
    EXPECT_EQ(light, dark);
}

TEST(SvgSceneWriterTest, AFullDiagramIsWellFormedAndUsesOnlyTinyElements)
{
    const FakeTextMeasurer measurer;
    const auto diagram = SequenceParser::parse(
        QStringLiteral("title T & <co>\nactor User\nUser -> Api : call\nApi -> Api : self\nalt ok\n"
                       "Api --> User : done\nelse fail\nApi -->> User : nope\nend\n== x ==\n"
                       "note over User, Api : n\n"));
    ASSERT_TRUE(diagram.isSuccess());
    const QString svg = SvgSceneWriter::write(
        SequenceLayout::layout(diagram.value(), DiagramStyle {}, 360, measurer), DiagramPalette {},
        3);

    const XmlSummary summary = summarize(svg);
    EXPECT_TRUE(summary.wellFormed);
    const QSet<QString> allowed { QStringLiteral("svg"),      QStringLiteral("g"),
                                  QStringLiteral("rect"),     QStringLiteral("line"),
                                  QStringLiteral("polyline"), QStringLiteral("polygon"),
                                  QStringLiteral("text") };
    EXPECT_TRUE(allowed.contains(summary.elements))
        << summary.elements.values().join(' ').toStdString();
    EXPECT_FALSE(svg.contains(QStringLiteral("nan")));
}
