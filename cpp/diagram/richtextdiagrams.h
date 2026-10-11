#pragma once

#include "diagram/diagramsvgprovider.h"
#include "diagram/embeddeddiagrams.h"

#include <QObject>
#include <QVariantMap>

#include <memory>
#include <vector>

namespace diagram
{
class RichTextDiagrams : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString html READ html WRITE setHtml NOTIFY inputChanged)
    Q_PROPERTY(int availableWidth READ availableWidth WRITE setAvailableWidth NOTIFY inputChanged)
    Q_PROPERTY(qreal pixelRatio READ pixelRatio WRITE setPixelRatio NOTIFY inputChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY inputChanged)
    Q_PROPERTY(int fontPixelSize READ fontPixelSize WRITE setFontPixelSize NOTIFY inputChanged)
    Q_PROPERTY(QVariantMap colors READ colors WRITE setColors NOTIFY inputChanged)
    Q_PROPERTY(QString renderedHtml READ renderedHtml NOTIFY renderedHtmlChanged)

public:
    explicit RichTextDiagrams(QObject* parent = nullptr);
    ~RichTextDiagrams() override;

    QString html() const { return m_html; }
    void setHtml(const QString& html);

    int availableWidth() const { return m_availableWidth; }
    void setAvailableWidth(int width);

    qreal pixelRatio() const { return m_pixelRatio; }
    void setPixelRatio(qreal ratio);

    QString fontFamily() const { return m_fontFamily; }
    void setFontFamily(const QString& family);

    int fontPixelSize() const { return m_fontPixelSize; }
    void setFontPixelSize(int size);

    QVariantMap colors() const { return m_colors; }
    void setColors(const QVariantMap& colors);

    QString renderedHtml() const { return m_renderedHtml; }

signals:
    void inputChanged();
    void renderedHtmlChanged();

private:
    void configure(DiagramSvgProvider& provider) const;
    void reconfigure();
    void compose();
    QString figure(const EmbeddedDiagram& diagram, const DiagramSvgProvider* provider) const;

    QString m_html;
    int m_availableWidth = 0;
    qreal m_pixelRatio = 3;
    QString m_fontFamily;
    int m_fontPixelSize = 0;
    QVariantMap m_colors;
    QList<EmbeddedDiagram> m_diagrams;
    std::vector<std::unique_ptr<DiagramSvgProvider>> m_providers;
    bool m_configuring = false;
    QString m_renderedHtml;
};
}
