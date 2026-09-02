#include "icongenerator.h"

#include <QDir>
#include <QFileInfo>
#include <QList>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QStringList>

namespace
{
const QString kSparkPath
    = QStringLiteral("M0 -1 C0.16 -0.36 0.36 -0.16 1 0 C0.36 0.16 0.16 0.36 0 1 "
                     "C-0.16 0.36 -0.36 0.16 -1 0 C-0.36 -0.16 -0.16 -0.36 0 -1 Z");

QString num(double value)
{
    QString text = QString::number(value, 'f', 2);
    if (text.contains(QLatin1Char('.')))
    {
        while (text.endsWith(QLatin1Char('0')))
            text.chop(1);
        if (text.endsWith(QLatin1Char('.')))
            text.chop(1);
    }
    if (text == QStringLiteral("-0"))
        text = QStringLiteral("0");
    return text;
}

QString filled(const QString& body)
{
    return QStringLiteral("<path d=\"%1\" fill=\"currentColor\" stroke=\"none\"/>").arg(body);
}

QString stroked(const QString& body) { return QStringLiteral("<path d=\"%1\"/>").arg(body); }

QString circlePath(double cx, double cy, double r)
{
    return QStringLiteral("M%1 %2 a%3 %3 0 1 0 %4 0 a%3 %3 0 1 0 -%4 0 Z")
        .arg(num(cx - r), num(cy), num(r), num(2 * r));
}

QString dot(double cx, double cy, double r) { return filled(circlePath(cx, cy, r)); }

int pickIndex(QRandomGenerator& rng, int count) { return int(rng.bounded(count)); }

QString pickOne(QRandomGenerator& rng, const QStringList& values)
{
    return values.at(pickIndex(rng, values.size()));
}

double pickOne(QRandomGenerator& rng, const QList<double>& values)
{
    return values.at(pickIndex(rng, values.size()));
}

enum class FrameKind
{
    None,
    RoundedRect,
    Circle,
    Hexagon,
    Shield,
    Document
};

enum MotifKind
{
    Bars,
    Lines,
    Spark,
    Bolt,
    Chevron,
    Arrow,
    Ring,
    Dots,
    Plus,
    Check,
    Wave,
    Play,
    MotifCount
};

struct Frame
{
    QString svg;
    double inner = 8.6;
    double cx = 12.0;
    double cy = 12.0;
};

struct Motif
{
    QString body;
    int rotations = 1;
};

Frame makeFrame(FrameKind kind, QRandomGenerator& rng)
{
    Frame frame;
    switch (kind)
    {
        case FrameKind::None:
            break;
        case FrameKind::RoundedRect:
            frame.svg
                = QStringLiteral("<rect x=\"3\" y=\"3\" width=\"18\" height=\"18\" rx=\"%1\"/>")
                      .arg(num(pickOne(rng, QList<double> { 2.0, 4.0, 6.0 })));
            frame.inner = 6.4;
            break;
        case FrameKind::Circle:
            frame.svg = QStringLiteral("<circle cx=\"12\" cy=\"12\" r=\"9\"/>");
            frame.inner = 5.8;
            break;
        case FrameKind::Hexagon:
            frame.svg
                = QStringLiteral("<path d=\"M12 3 L19.8 7.5 V16.5 L12 21 L4.2 16.5 V7.5 Z\"/>");
            frame.inner = 5.4;
            break;
        case FrameKind::Shield:
            frame.svg = QStringLiteral("<path d=\"M12 3 L20 6 V12.5 C20 17 16.5 20.2 12 21 "
                                       "C7.5 20.2 4 17 4 12.5 V6 Z\"/>");
            frame.inner = 4.8;
            frame.cy = 12.6;
            break;
        case FrameKind::Document:
            frame.svg = QStringLiteral("<path d=\"M6 3 H14.5 L19 7.5 V21 H6 Z\"/>"
                                       "<path d=\"M14.5 3 V7.5 H19\"/>");
            frame.inner = 4.4;
            frame.cx = 12.5;
            frame.cy = 14.2;
            break;
    }
    return frame;
}

Motif makeMotif(int kind, QRandomGenerator& rng)
{
    Motif motif;
    switch (kind)
    {
        case Bars:
            motif.body = stroked(
                pickOne(rng,
                        { QStringLiteral("M-0.8 0.9 V-0.1 M0 0.9 V-0.8 M0.8 0.9 V0.3"),
                          QStringLiteral("M-0.8 0.9 V0.3 M0 0.9 V-0.8 M0.8 0.9 V-0.2"),
                          QStringLiteral("M-0.8 0.9 V-0.5 M0 0.9 V0.15 M0.8 0.9 V-0.9") }));
            break;
        case Lines:
            motif.body = stroked(
                pickOne(rng,
                        { QStringLiteral("M-0.85 -0.8 H0.85 M-0.85 0 H0.85 M-0.85 0.8 H0.25"),
                          QStringLiteral("M-0.85 -0.8 H0.4 M-0.85 0 H0.85 M-0.85 0.8 H0.55"),
                          QStringLiteral("M-0.85 -0.55 H0.85 M-0.85 0.55 H0.85") }));
            break;
        case Spark:
            motif.body = filled(kSparkPath);
            break;
        case Bolt:
            motif.body
                = stroked(QStringLiteral("M0.3 -1 L-0.65 0.15 H-0.05 L-0.3 1 L0.65 -0.15 H0.05 Z"));
            break;
        case Chevron:
            motif.body = stroked(QStringLiteral("M-0.65 -0.4 L0 0.3 L0.65 -0.4"));
            motif.rotations = 4;
            break;
        case Arrow:
            motif.body = stroked(QStringLiteral("M0 0.85 V-0.85 M-0.55 -0.3 L0 -0.85 L0.55 -0.3"));
            motif.rotations = 4;
            break;
        case Ring:
            motif.body = stroked(circlePath(0, 0, 0.85)) + dot(0, 0, 0.26);
            break;
        case Dots:
            motif.body = dot(-0.55, -0.55, 0.26) + dot(0.55, -0.55, 0.26) + dot(-0.55, 0.55, 0.26)
                + dot(0.55, 0.55, 0.26);
            break;
        case Plus:
            motif.body = stroked(QStringLiteral("M0 -0.85 V0.85 M-0.85 0 H0.85"));
            break;
        case Check:
            motif.body = stroked(QStringLiteral("M-0.8 0.05 L-0.2 0.65 L0.8 -0.65"));
            break;
        case Wave:
            motif.body = stroked(QStringLiteral(
                "M-0.9 0.35 C-0.55 -0.75 -0.2 -0.75 0.1 0.15 C0.35 0.8 0.65 0.7 0.9 -0.25"));
            motif.rotations = 2;
            break;
        case Play:
            motif.body = stroked(QStringLiteral("M-0.5 -0.85 L0.85 0 L-0.5 0.85 Z"));
            motif.rotations = 4;
            break;
    }
    return motif;
}

QList<int> allowedMotifs(FrameKind frame)
{
    switch (frame)
    {
        case FrameKind::Document:
        case FrameKind::Shield:
            return { Bars, Lines, Spark, Check, Plus, Bolt };
        case FrameKind::None:
            return { Bars, Lines, Spark, Bolt, Arrow, Ring, Plus, Check, Wave, Play };
        default:
            break;
    }
    QList<int> all;
    for (int kind = 0; kind < MotifCount; ++kind)
        all.append(kind);
    return all;
}

QString accentSvg(QRandomGenerator& rng, double cx, double cy)
{
    switch (pickIndex(rng, 3))
    {
        case 0:
            return dot(cx, cy, 2.2);
        case 1:
            return QStringLiteral("<g transform=\"translate(%1 %2) scale(2.4)\">%3</g>")
                .arg(num(cx), num(cy), filled(kSparkPath));
        default:
            return QStringLiteral("<g transform=\"translate(%1 %2)\">%3</g>")
                .arg(num(cx), num(cy), stroked(QStringLiteral("M0 -2.1 V2.1 M-2.1 0 H2.1")));
    }
}
}

quint32 IconGenerator::seedFor(const QString& text)
{
    quint32 hash = 2166136261u;
    for (const char byte : text.toUtf8())
    {
        hash ^= quint8(byte);
        hash *= 16777619u;
    }
    return hash;
}

QString IconGenerator::svg(const QString& seed) { return svg(seedFor(seed)); }

QString IconGenerator::svg(quint32 seed)
{
    QRandomGenerator rng(seed);

    static const QList<FrameKind> frames { FrameKind::RoundedRect, FrameKind::RoundedRect,
                                           FrameKind::Circle,      FrameKind::Hexagon,
                                           FrameKind::Shield,      FrameKind::Document,
                                           FrameKind::None };
    const FrameKind frameKind = frames.at(pickIndex(rng, frames.size()));
    const Frame frame = makeFrame(frameKind, rng);

    const QList<int> motifs = allowedMotifs(frameKind);
    const int motifKind = motifs.at(pickIndex(rng, motifs.size()));
    const Motif motif = makeMotif(motifKind, rng);

    const bool framed = frameKind != FrameKind::None;
    const bool dense = motifKind == Bars || motifKind == Lines || motifKind == Dots;
    const double fill = framed
        ? pickOne(rng, dense ? QList<double> { 0.82, 0.9 } : QList<double> { 0.72, 0.8, 0.88 })
        : pickOne(rng, QList<double> { 0.85, 0.95 });
    const double scale = frame.inner * fill;
    const int angle = (360 / motif.rotations) * pickIndex(rng, motif.rotations);

    const bool accented = framed && rng.bounded(100) < 35;
    const bool accentTop = pickIndex(rng, 2) == 0 && frameKind != FrameKind::Document;
    const double shrink = accented ? 0.8 : 1.0;

    QString body = QStringLiteral("<g transform=\"translate(%1 %2) rotate(%3) scale(%4)\" "
                                  "stroke-width=\"%5\">%6</g>")
                       .arg(num(frame.cx), num(frame.cy), QString::number(angle), num(scale),
                            num(2.0 / (scale * shrink)), motif.body);
    body.prepend(frame.svg);

    if (accented)
    {
        const double accentY = accentTop ? 4.2 : 19.8;
        const double shiftY = accentTop ? 2.0 : -2.0;
        body = QStringLiteral("<g transform=\"translate(%1 %2) scale(%3) translate(-12 -12)\" "
                              "stroke-width=\"%4\">%5</g>%6")
                   .arg(num(12.0 - 2.0), num(12.0 + shiftY), num(shrink), num(2.0 / shrink), body,
                        accentSvg(rng, 19.8, accentY));
    }

    return QStringLiteral(
               "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\" "
               "stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" "
               "stroke-linejoin=\"round\">\n%1\n</svg>\n")
        .arg(body);
}

bool IconGenerator::writeTo(const QString& filePath, const QString& seed, QString* error)
{
    if (error)
        error->clear();

    const QString dir = QFileInfo(filePath).absolutePath();
    if (!QDir().mkpath(dir))
    {
        if (error)
            *error = QStringLiteral("Cannot create %1").arg(dir);
        return false;
    }

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        if (error)
            *error = QStringLiteral("Cannot write %1: %2").arg(filePath, file.errorString());
        return false;
    }

    file.write(svg(seed).toUtf8());
    if (!file.commit())
    {
        if (error)
            *error = QStringLiteral("Cannot write %1: %2").arg(filePath, file.errorString());
        return false;
    }

    return true;
}
