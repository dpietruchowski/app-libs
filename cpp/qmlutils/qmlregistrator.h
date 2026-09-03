#pragma once

#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QString>
#include <QUrl>
#include <memory>

#include "qmlutils/qmlsandbox.h"

#ifdef QML_LIVE_ENABLED
#include "qmllive/qmlsourceinterceptor.h"
#endif

class QmlRegistrator
{
public:
    QmlRegistrator(QQmlApplicationEngine& engine, const QString& uiRootDir,
                   const QString& moduleName);
    ~QmlRegistrator();

    template <typename Type>
    void registerSingletonInstance(const char* moduleName, const char* name, Type* value);
    template <typename Type> void registerSingletonInstance(const char* name, Type* value);
    template <typename Type> void registerType(const char* moduleName, const char* name);
    template <typename Type> void registerType(const char* name);

    void registerEnums(const QMetaObject& metaObject, const QString& name);
    void registerSingletonType(const QString& moduleName, const QString& qmlFile,
                               const QString& name);
    void registerSingletonType(const QString& qmlFile, const QString& name);
    void enableSourceReload(const QString& moduleSpec);
    void enableSandbox(const QString& moduleName, const QString& defaultDirectory);
    QUrl getMainQmlUrl();

private:
    QUrl resolveUrl(const QString& relativePath) const;

    QQmlApplicationEngine& m_engine;
    QString m_uiRootDir;
    QString m_moduleName;
    QmlSandbox* m_sandbox = nullptr;
#ifdef QML_LIVE_ENABLED
    std::unique_ptr<QmlSourceInterceptor> m_interceptor;
#endif
};

template <typename Type>
void QmlRegistrator::registerSingletonInstance(const char* moduleName, const char* name,
                                               Type* value)
{
    qmlRegisterSingletonInstance<Type>(moduleName, 1, 0, name, value);
}

template <typename Type>
void QmlRegistrator::registerSingletonInstance(const char* name, Type* value)
{
    registerSingletonInstance<Type>(m_moduleName.toUtf8().constData(), name, value);
}

template <typename Type> void QmlRegistrator::registerType(const char* moduleName, const char* name)
{
    qmlRegisterType<Type>(moduleName, 1, 0, name);
}

template <typename Type> void QmlRegistrator::registerType(const char* name)
{
    registerType<Type>(m_moduleName.toUtf8().constData(), name);
}
