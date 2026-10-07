#include "diagramsvgprovider.h"

#include "diagram/sequencelayout.h"
#include "diagram/sequenceparser.h"
#include "diagram/svgscenewriter.h"
#include "diagram/textmeasurer.h"

#include <QDebug>

#include <algorithm>
#include <cmath>
#include <utility>

namespace diagram
{
namespace
{
    using PaletteMember = QColor DiagramPalette::*;

    const QList<std::pair<QString, PaletteMember>>& paletteMembers()
    {
        static const QList<std::pair<QString, PaletteMember>> members {
            { QStringLiteral("background"), &DiagramPalette::background },
            { QStringLiteral("line"), &DiagramPalette::line },
            { QStringLiteral("text"), &DiagramPalette::text },
            { QStringLiteral("mutedText"), &DiagramPalette::mutedText },
            { QStringLiteral("boxFill"), &DiagramPalette::boxFill },
            { QStringLiteral("boxStroke"), &DiagramPalette::boxStroke },
            { QStringLiteral("noteFill"), &DiagramPalette::noteFill },
            { QStringLiteral("noteStroke"), &DiagramPalette::noteStroke },
            { QStringLiteral("groupStroke"), &DiagramPalette::groupStroke },
            { QStringLiteral("divider"), &DiagramPalette::divider },
        };
        return members;
    }

    DiagramPalette paletteFrom(const QVariantMap& colors)
    {
        DiagramPalette palette;
        for (const auto& [role, member] : paletteMembers())
        {
            const auto value = colors.constFind(role);
            if (value == colors.cend())
                continue;
            const QColor color = value->value<QColor>();
            if (!color.isValid())
            {
                qWarning() << "DiagramSvgProvider: invalid colour for" << role << *value;
                continue;
            }
            palette.*member = color;
        }
        return palette;
    }
}

DiagramSvgProvider::DiagramSvgProvider(QObject* parent)
    : QObject(parent)
{
}

void DiagramSvgProvider::setText(const QString& text)
{
    if (m_text == text)
        return;
    m_text = text;
    parse();
    emit textChanged();
}

void DiagramSvgProvider::setAvailableWidth(int width)
{
    const int clamped = std::max(0, width);
    if (m_availableWidth == clamped)
        return;
    const bool hadWidth = m_availableWidth > 0;
    m_availableWidth = clamped;
    if (layoutWidth() != m_laidOutWidth || hadWidth != (m_availableWidth > 0))
        layOut();
    else
        updateImageSize();
    emit availableWidthChanged();
}

void DiagramSvgProvider::setPixelRatio(qreal ratio)
{
    const qreal clamped = std::max<qreal>(1, ratio);
    if (qFuzzyCompare(m_pixelRatio, clamped))
        return;
    m_pixelRatio = clamped;
    write();
    emit pixelRatioChanged();
}

void DiagramSvgProvider::setFontFamily(const QString& family)
{
    if (m_style.fontFamily == family || family.isEmpty())
        return;
    m_style.fontFamily = family;
    layOut();
    emit fontFamilyChanged();
}

void DiagramSvgProvider::setFontPixelSize(int size)
{
    if (m_style.fontPixelSize == size || size <= 0)
        return;
    m_style.fontPixelSize = size;
    layOut();
    emit fontPixelSizeChanged();
}

void DiagramSvgProvider::setColors(const QVariantMap& colors)
{
    if (m_colors == colors)
        return;
    m_colors = colors;
    m_palette = paletteFrom(colors);
    write();
    emit colorsChanged();
}

void DiagramSvgProvider::parse()
{
    m_diagram.reset();
    if (m_text.trimmed().isEmpty())
    {
        setError(QString());
        setWarnings({});
    }
    else if (const auto parsed = SequenceParser::parse(m_text); parsed.isSuccess())
    {
        m_diagram = parsed.value();
        setError(QString());
        setWarnings(m_diagram->warnings);
    }
    else
    {
        setError(parsed.error());
        setWarnings({});
    }
    layOut();
}

int DiagramSvgProvider::layoutWidth() const
{
    return std::max(kWidthStep, m_availableWidth / kWidthStep * kWidthStep);
}

qreal DiagramSvgProvider::effectivePixelRatio() const
{
    const qreal area = m_scene->size.width() * m_scene->size.height();
    return std::clamp(std::sqrt(kMaxImagePixels / area), qreal(1), m_pixelRatio);
}

void DiagramSvgProvider::layOut()
{
    m_scene.reset();
    m_laidOutWidth = 0;
    if (m_diagram && m_availableWidth > 0)
    {
        const FontTextMeasurer measurer(m_style.font());
        m_scene = SequenceLayout::layout(*m_diagram, m_style, layoutWidth(), measurer);
        m_laidOutWidth = layoutWidth();
    }
    write();
    updateImageSize();
}

void DiagramSvgProvider::write()
{
    QString source;
    if (m_scene && !m_scene->size.isEmpty())
    {
        const QString svg = SvgSceneWriter::write(*m_scene, m_palette, effectivePixelRatio());
        source = QStringLiteral("data:image/svg+xml;utf8,")
            + QString::fromLatin1(svg.toUtf8().toPercentEncoding());
    }
    if (source == m_svgSource)
        return;
    m_svgSource = source;
    emit svgSourceChanged();
}

void DiagramSvgProvider::updateImageSize()
{
    int width = 0;
    int height = 0;
    if (m_scene && !m_scene->size.isEmpty())
    {
        const QSizeF scene = m_scene->size;
        width = std::min(m_availableWidth, int(std::ceil(scene.width())));
        height = qRound(scene.height() * qreal(width) / scene.width());
    }
    if (width == m_imageWidth && height == m_imageHeight)
        return;
    m_imageWidth = width;
    m_imageHeight = height;
    emit imageSizeChanged();
}

void DiagramSvgProvider::setError(const QString& error)
{
    if (m_error == error)
        return;
    m_error = error;
    emit errorChanged();
}

void DiagramSvgProvider::setWarnings(const QStringList& warnings)
{
    if (m_warnings == warnings)
        return;
    m_warnings = warnings;
    emit warningsChanged();
}
}
