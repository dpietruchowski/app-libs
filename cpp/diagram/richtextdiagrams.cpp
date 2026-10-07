#include "richtextdiagrams.h"

#include <QScopedValueRollback>

#include <utility>

namespace diagram
{
RichTextDiagrams::RichTextDiagrams(QObject* parent)
    : QObject(parent)
{
}

RichTextDiagrams::~RichTextDiagrams() = default;

void RichTextDiagrams::setHtml(const QString& html)
{
    if (m_html == html)
        return;
    m_html = html;
    m_diagrams = EmbeddedDiagrams::find(html);
    {
        const QScopedValueRollback configuring(m_configuring, true);
        std::size_t index = 0;
        for (const EmbeddedDiagram& diagram : std::as_const(m_diagrams))
        {
            if (!diagram.closed)
                continue;
            if (index == m_providers.size())
            {
                auto provider = std::make_unique<DiagramSvgProvider>();
                configure(*provider);
                for (const auto signal :
                     { &DiagramSvgProvider::svgSourceChanged, &DiagramSvgProvider::imageSizeChanged,
                       &DiagramSvgProvider::errorChanged })
                    connect(provider.get(), signal, this, &RichTextDiagrams::compose);
                m_providers.push_back(std::move(provider));
            }
            m_providers.at(index)->setText(diagram.text);
            ++index;
        }
        m_providers.resize(index);
    }
    emit inputChanged();
    compose();
}

void RichTextDiagrams::setAvailableWidth(int width)
{
    if (m_availableWidth == width)
        return;
    m_availableWidth = width;
    emit inputChanged();
    reconfigure();
}

void RichTextDiagrams::setPixelRatio(qreal ratio)
{
    if (qFuzzyCompare(m_pixelRatio, ratio))
        return;
    m_pixelRatio = ratio;
    emit inputChanged();
    reconfigure();
}

void RichTextDiagrams::setFontFamily(const QString& family)
{
    if (m_fontFamily == family)
        return;
    m_fontFamily = family;
    emit inputChanged();
    reconfigure();
}

void RichTextDiagrams::setFontPixelSize(int size)
{
    if (m_fontPixelSize == size)
        return;
    m_fontPixelSize = size;
    emit inputChanged();
    reconfigure();
}

void RichTextDiagrams::setColors(const QVariantMap& colors)
{
    if (m_colors == colors)
        return;
    m_colors = colors;
    emit inputChanged();
    reconfigure();
}

void RichTextDiagrams::configure(DiagramSvgProvider& provider) const
{
    provider.setAvailableWidth(m_availableWidth);
    provider.setPixelRatio(m_pixelRatio);
    provider.setFontFamily(m_fontFamily);
    provider.setFontPixelSize(m_fontPixelSize);
    provider.setColors(m_colors);
}

void RichTextDiagrams::reconfigure()
{
    {
        const QScopedValueRollback configuring(m_configuring, true);
        for (const auto& provider : m_providers)
            configure(*provider);
    }
    compose();
}

void RichTextDiagrams::compose()
{
    if (m_configuring)
        return;
    QString rendered;
    qsizetype copied = 0;
    std::size_t index = 0;
    for (const EmbeddedDiagram& diagram : std::as_const(m_diagrams))
    {
        const DiagramSvgProvider* provider
            = diagram.closed ? m_providers.at(index++).get() : nullptr;
        rendered += QStringView(m_html).mid(copied, diagram.start - copied);
        rendered += figure(diagram, provider);
        copied = diagram.end;
    }
    rendered += QStringView(m_html).mid(copied);
    if (rendered == m_renderedHtml)
        return;
    m_renderedHtml = rendered;
    emit renderedHtmlChanged();
}

QString RichTextDiagrams::figure(const EmbeddedDiagram& diagram,
                                 const DiagramSvgProvider* provider) const
{
    const QString errorParagraph = QStringLiteral("<p class=\"diagramError\">Diagram: %1</p>");
    if (provider == nullptr)
        return errorParagraph.arg(QStringLiteral("@startuml without @enduml"));
    if (!provider->error().isEmpty())
        return errorParagraph.arg(
            EmbeddedDiagrams::locate(diagram, provider->error()).message.toHtmlEscaped());
    if (provider->svgSource().isEmpty())
        return {};
    return QStringLiteral("<p class=\"diagram\" align=\"center\"><img src=\"%1\" width=\"%2\" "
                          "height=\"%3\"></p>")
        .arg(provider->svgSource(), QString::number(provider->imageWidth()),
             QString::number(provider->imageHeight()));
}
}
