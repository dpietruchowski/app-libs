#pragma once

#include "diagram/diagrampalette.h"
#include "diagram/diagramstyle.h"
#include "diagram/scene.h"
#include "diagram/sequencediagram.h"

#include <QObject>
#include <QStringList>
#include <QVariantMap>

#include <optional>

namespace diagram
{
class DiagramSvgProvider : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(
        int availableWidth READ availableWidth WRITE setAvailableWidth NOTIFY availableWidthChanged)
    Q_PROPERTY(qreal pixelRatio READ pixelRatio WRITE setPixelRatio NOTIFY pixelRatioChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY fontFamilyChanged)
    Q_PROPERTY(
        int fontPixelSize READ fontPixelSize WRITE setFontPixelSize NOTIFY fontPixelSizeChanged)
    Q_PROPERTY(QVariantMap colors READ colors WRITE setColors NOTIFY colorsChanged)
    Q_PROPERTY(QString svgSource READ svgSource NOTIFY svgSourceChanged)
    Q_PROPERTY(int imageWidth READ imageWidth NOTIFY imageSizeChanged)
    Q_PROPERTY(int imageHeight READ imageHeight NOTIFY imageSizeChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QStringList warnings READ warnings NOTIFY warningsChanged)

public:
    static constexpr int kWidthStep = 16;
    static constexpr qreal kMaxImagePixels = 8'000'000;

    explicit DiagramSvgProvider(QObject* parent = nullptr);

    QString text() const { return m_text; }
    void setText(const QString& text);

    int availableWidth() const { return m_availableWidth; }
    void setAvailableWidth(int width);

    qreal pixelRatio() const { return m_pixelRatio; }
    void setPixelRatio(qreal ratio);

    QString fontFamily() const { return m_style.fontFamily; }
    void setFontFamily(const QString& family);

    int fontPixelSize() const { return m_style.fontPixelSize; }
    void setFontPixelSize(int size);

    QVariantMap colors() const { return m_colors; }
    void setColors(const QVariantMap& colors);

    QString svgSource() const { return m_svgSource; }
    int imageWidth() const { return m_imageWidth; }
    int imageHeight() const { return m_imageHeight; }
    QString error() const { return m_error; }
    QStringList warnings() const { return m_warnings; }

signals:
    void textChanged();
    void availableWidthChanged();
    void pixelRatioChanged();
    void fontFamilyChanged();
    void fontPixelSizeChanged();
    void colorsChanged();
    void svgSourceChanged();
    void imageSizeChanged();
    void errorChanged();
    void warningsChanged();

private:
    void parse();
    void layOut();
    void write();
    void updateImageSize();
    int layoutWidth() const;
    qreal effectivePixelRatio() const;
    void setError(const QString& error);
    void setWarnings(const QStringList& warnings);

    QString m_text;
    int m_availableWidth = 0;
    qreal m_pixelRatio = 3;
    DiagramStyle m_style;
    QVariantMap m_colors;
    DiagramPalette m_palette;
    std::optional<SequenceDiagram> m_diagram;
    std::optional<Scene> m_scene;
    int m_laidOutWidth = 0;
    QString m_svgSource;
    int m_imageWidth = 0;
    int m_imageHeight = 0;
    QString m_error;
    QStringList m_warnings;
};
}
