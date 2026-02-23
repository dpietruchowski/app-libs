#include "context.h"
#include "resources.h"

#include <QCoreApplication>

namespace android
{

Context Context::application()
{
    return Context(QJniObject(QNativeInterface::QAndroidApplication::context()));
}

Context::Context(QJniObject jni)
    : m_context(std::move(jni))
{
}

Resources Context::resources() const
{
    return Resources(
        m_context.callObjectMethod("getResources", "()Landroid/content/res/Resources;"));
}

QString Context::packageName() const
{
    return m_context.callObjectMethod("getPackageName", "()Ljava/lang/String;").toString();
}

QJniObject Context::systemService(const QString& name) const
{
    return m_context.callObjectMethod("getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;",
                                      QJniObject::fromString(name).object<jstring>());
}

QJniObject Context::jniObject() const { return m_context; }

}  // namespace android
