#include "qmlregistrator.h"
#include <QQmlContext>
#include <QQmlEngine>

QmlRegistrator::QmlRegistrator(QQmlApplicationEngine& engine, const QString& uiRootDir,
                               const QString& moduleName)
    : m_engine(engine)
    , m_uiRootDir(uiRootDir)
    , m_moduleName(moduleName)
{

    m_engine.addImportPath("qrc:/");
}

QmlRegistrator::~QmlRegistrator() = default;

QUrl QmlRegistrator::resolveUrl(const QString& relativePath) const
{
    QUrl url;
    if (m_uiRootDir.startsWith("qrc:"))
    {
        url = QUrl(m_uiRootDir + relativePath);
    }
    else
    {
        url = QUrl::fromLocalFile(m_uiRootDir + relativePath);
    }

#ifdef QML_LIVE_ENABLED
    if (m_interceptor)
    {
        return m_interceptor->mapUrl(url);
    }
#endif
    return url;
}

void QmlRegistrator::registerSingletonType(const QString& moduleName, const QString& qmlFile,
                                           const QString& name)
{
    const QUrl url = resolveUrl(qmlFile);
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

void QmlRegistrator::enableSourceReload(const QString& moduleSpec)
{
#ifdef QML_LIVE_ENABLED
    auto interceptor = std::make_unique<QmlSourceInterceptor>();
    interceptor->addModules(moduleSpec);
    if (interceptor->isEmpty())
    {
        return;
    }

    m_interceptor = std::move(interceptor);
    m_engine.addUrlInterceptor(m_interceptor.get());
#else
    Q_UNUSED(moduleSpec)
#endif
}

QUrl QmlRegistrator::getMainQmlUrl() { return resolveUrl("Main.qml"); }
