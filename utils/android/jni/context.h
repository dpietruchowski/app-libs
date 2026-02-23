#pragma once

#include <QJniObject>
#include <QString>

namespace android
{

class Resources;

class Context final
{
public:
    static Context application();

    Resources resources() const;
    QString packageName() const;
    QJniObject systemService(const QString& name) const;

    QJniObject jniObject() const;

private:
    explicit Context(QJniObject jni);

    QJniObject m_context;
};

}  // namespace android
