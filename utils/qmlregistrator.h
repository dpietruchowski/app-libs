#pragma once

#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QString>
#include <QUrl>

#ifdef QML_LIVE_ENABLED
#include "lib/filewatcher.h"
#endif

class QmlRegistrator
{
public:
    QmlRegistrator(QQmlApplicationEngine& engine, const QString& uiRootDir,
                   const QString& moduleName);

    template <typename Type> void registerSingletonInstance(const char* name, Type* value);
    template <typename Type> void registerType(const char* moduleName, const char* name);
    template <typename Type> void registerType(const char* name);

    void registerEnums(const QMetaObject& metaObject, const QString& name);
    void registerSingletonType(const QString& moduleName, const QString& qmlFile,
                               const QString& name);
    void registerSingletonType(const QString& qmlFile, const QString& name);
    void setupLiveReload(const QString& loaderName = "mainLoader",
                         const QString& mainViewFile = "MainView.qml");
    QUrl getMainQmlUrl();

private:
    QQmlApplicationEngine& m_engine;
    QString m_uiRootDir;
    QString m_moduleName;
#ifdef QML_LIVE_ENABLED
    std::unique_ptr<FileWatcher> m_watcher;
    void generateQmlDir();
#endif
};

template <typename Type>
void QmlRegistrator::registerSingletonInstance(const char* name, Type* value)
{
#ifdef QML_LIVE_ENABLED
    QQmlContext* ctx = m_engine.rootContext();
    ctx->setContextProperty(name, value);
#else
    qmlRegisterSingletonInstance<Type>(m_moduleName.toUtf8().constData(), 1, 0, name, value);
#endif
}

template <typename Type> void QmlRegistrator::registerType(const char* moduleName, const char* name)
{
    qmlRegisterType<Type>(moduleName, 1, 0, name);
}

template <typename Type> void QmlRegistrator::registerType(const char* name)
{
    registerType<Type>(m_moduleName.toUtf8().constData(), name);
}
