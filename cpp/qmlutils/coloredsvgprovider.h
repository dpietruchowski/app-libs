#pragma once
#include <QColor>
#include <QMap>
#include <QObject>
#include <QString>

class ColoredSvgProvider : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString svgSource READ svgSource NOTIFY svgSourceChanged)
    Q_PROPERTY(QString svgOriginSource READ svgOriginSource WRITE setSvgOriginSource NOTIFY
                   svgOriginSourceChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
    Q_PROPERTY(QString icon READ icon WRITE setIcon NOTIFY iconChanged)
    Q_PROPERTY(int width READ width WRITE setWidth NOTIFY widthChanged)
    Q_PROPERTY(int height READ height WRITE setHeight NOTIFY heightChanged)

public:
    explicit ColoredSvgProvider(QObject* parent = nullptr);

    QString svgSource() const { return m_svgSource; }
    QString svgOriginSource() const { return m_svgOriginSource; }
    void setSvgOriginSource(const QString& s);

    QColor color() const { return m_color; }
    void setColor(const QColor& c);

    QString icon() const { return m_icon; }
    void setIcon(const QString& s);

    int width() const { return m_width; }
    void setWidth(int w);

    int height() const { return m_height; }
    void setHeight(int h);

signals:
    void svgSourceChanged();
    void svgOriginSourceChanged();
    void colorChanged();
    void iconChanged();
    void widthChanged();
    void heightChanged();

private:
    void updateSvgSource();
    QString loadSvgContent(const QString& path);

    QString m_svgSource;
    QString m_svgOriginSource;
    QString m_icon;
    QColor m_color;
    int m_width = 24;
    int m_height = 24;
    static QMap<QString, QString> s_svgCache;
};
