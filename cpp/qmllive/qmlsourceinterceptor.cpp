#include "qmlsourceinterceptor.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>

namespace
{
QString resourcePath(const QUrl& url)
{
    if (url.scheme() == QLatin1String("qrc"))
    {
        return url.path();
    }
    if (url.scheme().isEmpty() && url.path().startsWith(QLatin1Char(':')))
    {
        return url.path().mid(1);
    }
    return QString();
}
}

void QmlSourceInterceptor::addModule(const QString& uri, const QString& sourceDir)
{
    const QFileInfo info(sourceDir);
    if (!info.isDir())
    {
        qWarning() << "QML live: source directory does not exist:" << sourceDir;
        return;
    }

    Module module;
    module.resourcePrefix = QLatin1Char('/')
        + QString(uri).replace(QLatin1Char('.'), QLatin1Char('/')) + QLatin1Char('/');
    module.sourceDir = info.absoluteFilePath();
    m_modules.append(module);

    qInfo() << "QML live:" << module.resourcePrefix << "->" << module.sourceDir;
}

void QmlSourceInterceptor::addModules(const QString& spec)
{
    const QStringList entries = spec.split(QLatin1Char('|'), Qt::SkipEmptyParts);
    for (const QString& entry : entries)
    {
        const qsizetype separator = entry.indexOf(QLatin1Char('='));
        if (separator <= 0)
        {
            qWarning() << "QML live: malformed module mapping:" << entry;
            continue;
        }
        addModule(entry.left(separator), entry.mid(separator + 1));
    }
}

bool QmlSourceInterceptor::isEmpty() const { return m_modules.isEmpty(); }

QStringList QmlSourceInterceptor::sourceDirs() const
{
    QStringList dirs;
    for (const Module& module : m_modules)
    {
        dirs.append(module.sourceDir);
    }
    return dirs;
}

QUrl QmlSourceInterceptor::mapUrl(const QUrl& url) const
{
    const QString path = resourcePath(url);
    if (path.isEmpty())
    {
        return url;
    }

    for (const Module& module : m_modules)
    {
        if (!path.startsWith(module.resourcePrefix))
        {
            continue;
        }

        const QString relative = path.mid(module.resourcePrefix.size());
        const QString candidate = QDir::cleanPath(module.sourceDir + QLatin1Char('/') + relative);
        if (QFileInfo::exists(candidate))
        {
            return QUrl::fromLocalFile(candidate);
        }
    }

    return url;
}

QUrl QmlSourceInterceptor::intercept(const QUrl& url, DataType type)
{
    if (type == QmldirFile)
    {
        return url;
    }
    return mapUrl(url);
}
