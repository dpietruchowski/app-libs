#pragma once

#include <QObject>
#include <QSize>
#include <QString>
#include <QUrl>

class RichTextImages : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString html READ html WRITE setHtml NOTIFY inputChanged)
    Q_PROPERTY(QUrl baseUrl READ baseUrl WRITE setBaseUrl NOTIFY inputChanged)
    Q_PROPERTY(qreal maxWidth READ maxWidth WRITE setMaxWidth NOTIFY inputChanged)
    Q_PROPERTY(
        qreal sourcePixelRatio READ sourcePixelRatio WRITE setSourcePixelRatio NOTIFY inputChanged)
    Q_PROPERTY(QString variantSuffix READ variantSuffix WRITE setVariantSuffix NOTIFY inputChanged)
    Q_PROPERTY(QString fittedHtml READ fittedHtml NOTIFY fittedHtmlChanged)

public:
    struct Options
    {
        QUrl baseUrl;
        qreal maxWidth = 0;
        qreal sourcePixelRatio = 1;
        QString variantSuffix;
    };

    explicit RichTextImages(QObject* parent = nullptr);

    static QString fit(const QString& html, const Options& options);
    static QSize imageSize(const QUrl& url);

    QString html() const { return m_html; }
    void setHtml(const QString& html);

    QUrl baseUrl() const { return m_options.baseUrl; }
    void setBaseUrl(const QUrl& baseUrl);

    qreal maxWidth() const { return m_options.maxWidth; }
    void setMaxWidth(qreal maxWidth);

    qreal sourcePixelRatio() const { return m_options.sourcePixelRatio; }
    void setSourcePixelRatio(qreal sourcePixelRatio);

    QString variantSuffix() const { return m_options.variantSuffix; }
    void setVariantSuffix(const QString& variantSuffix);

    QString fittedHtml() const { return m_fittedHtml; }

signals:
    void inputChanged();
    void fittedHtmlChanged();

private:
    void refit();

    QString m_html;
    Options m_options;
    QString m_fittedHtml;
};
