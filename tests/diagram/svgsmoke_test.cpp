#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <gtest/gtest.h>

#include "diagram/sequencelayout.h"
#include "diagram/sequenceparser.h"
#include "diagram/svgscenewriter.h"

using namespace diagram;

TEST(SvgSmokeTest, QtSvgRendersTheWriterOutput)
{
    const DiagramStyle style;
    const FontTextMeasurer measurer(style.font());
    const auto diagram = SequenceParser::parse(QStringLiteral(
        "actor User\nparticipant \"Web app\" as Web\nUser -> Web : sign in\n"
        "Web -> Web : check\nalt ok\nWeb --> User : welcome\nelse\nWeb -->> User : retry\nend\n"
        "== later ==\nnote right of Web : expires\n"));
    ASSERT_TRUE(diagram.isSuccess());
    const Scene scene = SequenceLayout::layout(diagram.value(), style, 360, measurer);
    const DiagramPalette palette;
    const QString svg = SvgSceneWriter::write(scene, palette, 3);

    QSvgRenderer renderer(svg.toUtf8());
    ASSERT_TRUE(renderer.isValid());
    EXPECT_EQ(renderer.defaultSize(), (scene.size * 3).toSize());
    EXPECT_EQ(renderer.viewBoxF(), QRectF(QPointF(0, 0), scene.size));

    QImage image(renderer.defaultSize(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    renderer.render(&painter);
    painter.end();

    EXPECT_EQ(image.pixelColor(1, 1), palette.background);
    qsizetype inked = 0;
    for (int y = 0; y < image.height(); y += 3)
    {
        for (int x = 0; x < image.width(); x += 3)
            inked += image.pixelColor(x, y) != palette.background;
    }
    EXPECT_GT(inked, image.width() * image.height() / 9 / 50);
}
