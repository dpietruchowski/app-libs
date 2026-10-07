#include <QFile>
#include <QGuiApplication>
#include <QTextStream>

#include "diagram/sequencelayout.h"
#include "diagram/sequenceparser.h"
#include "diagram/svgscenewriter.h"

using namespace diagram;

namespace
{
DiagramPalette nightPalette()
{
    DiagramPalette palette;
    palette.background = QColor(0x1e, 0x1f, 0x24);
    palette.line = QColor(0xb0, 0xb4, 0xc0);
    palette.text = QColor(0xe8, 0xe8, 0xec);
    palette.mutedText = QColor(0xa0, 0xa4, 0xb0);
    palette.boxFill = QColor(0x2c, 0x31, 0x40);
    palette.boxStroke = QColor(0x8a, 0x96, 0xb8);
    palette.noteFill = QColor(0x3a, 0x34, 0x22);
    palette.noteStroke = QColor(0xa8, 0x92, 0x50);
    palette.groupStroke = QColor(0x80, 0x84, 0x90);
    palette.divider = QColor(0x80, 0x84, 0x90);
    return palette;
}
}

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    QTextStream err(stderr);

    const QStringList args = app.arguments();
    if (args.size() < 3)
    {
        err << "usage: diagram_render <input.puml> <output.svg> [width] [light|night] "
               "[pixelRatio]\n";
        return 2;
    }

    QFile input(args.at(1));
    if (!input.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        err << "cannot read " << args.at(1) << "\n";
        return 1;
    }
    const auto diagram = SequenceParser::parse(QString::fromUtf8(input.readAll()));
    if (diagram.isFailure())
    {
        err << diagram.error() << "\n";
        return 1;
    }
    for (const QString& warning : diagram.value().warnings)
        err << "warning: " << warning << "\n";

    const qreal width = args.size() > 3 ? args.at(3).toDouble() : 360;
    const bool night = args.size() > 4 && args.at(4) == QStringLiteral("night");
    const qreal pixelRatio = args.size() > 5 ? args.at(5).toDouble() : 1;
    const DiagramStyle style;
    const FontTextMeasurer measurer(style.font());
    const Scene scene = SequenceLayout::layout(diagram.value(), style, width, measurer);
    const QString svg
        = SvgSceneWriter::write(scene, night ? nightPalette() : DiagramPalette {}, pixelRatio);

    QFile output(args.at(2));
    if (!output.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        err << "cannot write " << args.at(2) << "\n";
        return 1;
    }
    output.write(svg.toUtf8());
    err << "scale at " << width << ": " << width / scene.size.width() << "\n";
    return 0;
}
