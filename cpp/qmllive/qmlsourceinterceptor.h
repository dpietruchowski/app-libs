#pragma once

#include <QList>
#include <QQmlAbstractUrlInterceptor>
#include <QString>
#include <QStringList>
#include <QUrl>

class QmlSourceInterceptor : public QQmlAbstractUrlInterceptor
{
public:
    void addModule(const QString& uri, const QString& sourceDir);
    void addModules(const QString& spec);

    bool isEmpty() const;
    QStringList sourceDirs() const;
    QUrl mapUrl(const QUrl& url) const;

    QUrl intercept(const QUrl& url, DataType type) override;

private:
    struct Module
    {
        QString resourcePrefix;
        QString sourceDir;
    };

    QList<Module> m_modules;
};
