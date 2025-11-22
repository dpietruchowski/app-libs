#include "qmlregistrator.h"
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTextStream>

QmlRegistrator::QmlRegistrator(QQmlApplicationEngine& engine, const QString& uiRootDir,
                               const QString& moduleName)
    : m_engine(engine)
    , m_uiRootDir(uiRootDir)
    , m_moduleName(moduleName)
{

    m_engine.addImportPath("qrc:/");
}

void QmlRegistrator::registerSingletonType(const QString& moduleName, const QString& qmlFile,
                                           const QString& name)
{
    QUrl url;
    if (m_uiRootDir.startsWith("qrc:"))
    {
        url = QUrl(m_uiRootDir + "/" + qmlFile);
    }
    else
    {
        url = QUrl::fromLocalFile(m_uiRootDir + "/" + qmlFile);
    }
    qmlRegisterSingletonType(url, moduleName.toUtf8().constData(), 1, 0, name.toUtf8().constData());
}

void QmlRegistrator::registerSingletonType(const QString& qmlFile, const QString& name)
{
    registerSingletonType(m_moduleName, qmlFile, name);
}

void QmlRegistrator::registerEnums(const QMetaObject& metaObject, const QString& name)
{
    qmlRegisterUncreatableMetaObject(metaObject, m_moduleName.toUtf8().constData(), 1, 0,
                                     name.toUtf8().constData(),
                                     QString("Cannot create %1 namespace - it only contains enums")
                                         .arg(name)
                                         .toUtf8()
                                         .constData());
}

#ifdef QML_LIVE_ENABLED
void QmlRegistrator::generateQmlDir()
{
    QDir watchDir(m_uiRootDir);
    QString qmlDirPath = watchDir.absoluteFilePath("qmldir");

    QFile qmlDirFile(qmlDirPath);
    if (!qmlDirFile.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qDebug() << "Failed to create qmldir file:" << qmlDirPath;
        return;
    }

    QTextStream out(&qmlDirFile);
    out << "module " << m_moduleName << "\n";

    QDirIterator it(watchDir.absolutePath(), QStringList() << "*.qml", QDir::Files,
                    QDirIterator::Subdirectories);

    while (it.hasNext())
    {
        QString filePath = it.next();
        QFileInfo fileInfo(filePath);
        QString componentName = fileInfo.baseName();
        QString relativePath = watchDir.relativeFilePath(filePath);

        out << componentName << " 1.0 " << relativePath << "\n";
        qDebug() << "Registered QML component:" << componentName << "->" << relativePath;
    }

    qmlDirFile.close();
    qDebug() << "Generated qmldir file:" << qmlDirPath;
}
#endif

void QmlRegistrator::setupLiveReload()
{
#ifdef QML_LIVE_ENABLED
    qDebug() << "Using qml live";
    QDir watchDir(m_uiRootDir);

    generateQmlDir();

    m_engine.addImportPath(watchDir.absolutePath());
    qDebug() << "Adding import path:" << watchDir.absolutePath();

    QDirIterator it(watchDir.absolutePath(), QDir::Dirs | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        QString subDir = it.next();
        m_engine.addImportPath(subDir);
        qDebug() << "Adding import path:" << subDir;
    }

    FileWatcher* watcher = new FileWatcher(
        [this, watchDir, loaderName, mainViewFile]()
        {
            QObject* root
                = m_engine.rootObjects().isEmpty() ? nullptr : m_engine.rootObjects().first();
            if (!root)
                return;

            QObject* loader = root->findChild<QObject*>(loaderName.toUtf8().constData());
            if (!loader)
                return;

            QUrl viewUrl = QUrl::fromLocalFile(watchDir.filePath(mainViewFile));

            m_engine.clearComponentCache();
            loader->setProperty("source", QUrl());
            loader->setProperty("source", viewUrl);
        });

    watcher->setDirectory(watchDir.absolutePath());
    m_watcher.reset(watcher);
#endif
}

QUrl QmlRegistrator::getMainQmlUrl()
{
    if (m_uiRootDir.startsWith("qrc:"))
    {
        return QUrl(m_uiRootDir + "/Main.qml");
    }
    else
    {
        return QUrl::fromLocalFile(m_uiRootDir + "/Main.qml");
    }
}
