#include "richtextimages.h"

#include <QHash>
#include <QImageReader>
#include <QRegularExpression>

#include <cmath>

namespace
{
QString localPath(const QUrl& url)
{
    if (url.scheme() == QLatin1String("qrc"))
        return QLatin1Char(':') + url.path();
    if (url.isLocalFile())
        return url.toLocalFile();
    return {};
}

QString withVariant(const QString& source, const QString& suffix)
{
    const qsizetype dot = source.lastIndexOf(QLatin1Char('.'));
    const qsizetype slash = source.lastIndexOf(QLatin1Char('/'));
    if (dot <= slash)
        return source + suffix;
    return source.left(dot) + suffix + source.mid(dot);
}
}

RichTextImages::RichTextImages(QObject* parent)
    : QObject(parent)
{
}

QSize RichTextImages::imageSize(const QUrl& url)
{
    static QHash<QUrl, QSize> sizes;
    const auto cached = sizes.constFind(url);
    if (cached != sizes.constEnd())
        return *cached;
    const QString path = localPath(url);
    const QSize size = path.isEmpty() ? QSize() : QImageReader(path).size();
    sizes.insert(url, size);
    return size;
}

QString RichTextImages::fit(const QString& html, const Options& options)
{
    static const QRegularExpression imageTag(
        QStringLiteral(R"tag(<img\b([^>]*?)\bsrc\s*=\s*"([^"]+)"([^>]*)>)tag"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression widthAttribute(QStringLiteral(R"(\bwidth\s*=)"),
                                                   QRegularExpression::CaseInsensitiveOption);

    QString result;
    qsizetype copied = 0;
    auto matches = imageTag.globalMatch(html);
    while (matches.hasNext())
    {
        const auto match = matches.next();
        const QString leading = match.captured(1).trimmed();
        const QString before = leading.isEmpty() ? QString() : QLatin1Char(' ') + leading;
        const QString after = match.captured(3);
        QString source = match.captured(2);

        if (!options.variantSuffix.isEmpty())
        {
            const QString variant = withVariant(source, options.variantSuffix);
            if (imageSize(options.baseUrl.resolved(QUrl(variant))).isValid())
                source = variant;
        }

        QString width;
        const QSize size = imageSize(options.baseUrl.resolved(QUrl(source)));
        if (size.isValid() && !widthAttribute.match(before + after).hasMatch())
        {
            qreal natural = size.width() / std::max(options.sourcePixelRatio, qreal(1));
            if (options.maxWidth > 0)
                natural = std::min(natural, options.maxWidth);
            width = QStringLiteral(" width=\"%1\"").arg(static_cast<int>(std::floor(natural)));
        }

        result += QStringView(html).mid(copied, match.capturedStart() - copied);
        result += QStringLiteral("<img%1%2 src=\"%3\"%4>").arg(before, width, source, after);
        copied = match.capturedEnd();
    }
    result += QStringView(html).mid(copied);
    return result;
}

void RichTextImages::setHtml(const QString& html)
{
    if (m_html == html)
        return;
    m_html = html;
    emit inputChanged();
    refit();
}

void RichTextImages::setBaseUrl(const QUrl& baseUrl)
{
    if (m_options.baseUrl == baseUrl)
        return;
    m_options.baseUrl = baseUrl;
    emit inputChanged();
    refit();
}

void RichTextImages::setMaxWidth(qreal maxWidth)
{
    if (qFuzzyCompare(m_options.maxWidth, maxWidth))
        return;
    m_options.maxWidth = maxWidth;
    emit inputChanged();
    refit();
}

void RichTextImages::setSourcePixelRatio(qreal sourcePixelRatio)
{
    if (qFuzzyCompare(m_options.sourcePixelRatio, sourcePixelRatio))
        return;
    m_options.sourcePixelRatio = sourcePixelRatio;
    emit inputChanged();
    refit();
}

void RichTextImages::setVariantSuffix(const QString& variantSuffix)
{
    if (m_options.variantSuffix == variantSuffix)
        return;
    m_options.variantSuffix = variantSuffix;
    emit inputChanged();
    refit();
}

void RichTextImages::refit()
{
    const QString fitted = fit(m_html, m_options);
    if (fitted == m_fittedHtml)
        return;
    m_fittedHtml = fitted;
    emit fittedHtmlChanged();
}
