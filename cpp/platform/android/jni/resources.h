#pragma once

#include <QJniObject>
#include <QString>

namespace android
{

class Context;

class Resources final
{
public:
    int identifier(const QString& name, const QString& defType, const QString& defPackage) const;

    QJniObject jniObject() const;

private:
    friend class Context;
    explicit Resources(QJniObject jni);

    QJniObject m_resources;
};

}  // namespace android
