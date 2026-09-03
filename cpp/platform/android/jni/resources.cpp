#include "resources.h"

namespace android
{

Resources::Resources(QJniObject jni)
    : m_resources(std::move(jni))
{
}

int Resources::identifier(const QString& name, const QString& defType,
                          const QString& defPackage) const
{
    return m_resources.callMethod<jint>("getIdentifier",
                                        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)I",
                                        QJniObject::fromString(name).object<jstring>(),
                                        QJniObject::fromString(defType).object<jstring>(),
                                        QJniObject::fromString(defPackage).object<jstring>());
}

QJniObject Resources::jniObject() const { return m_resources; }

}  // namespace android
