#include "notificationchannel.h"

namespace android
{

NotificationChannel::NotificationChannel(const QString& id, const QString& name, int importance)
    : m_channel("android/app/NotificationChannel", "(Ljava/lang/String;Ljava/lang/CharSequence;I)V",
                QJniObject::fromString(id).object<jstring>(),
                QJniObject::fromString(name).object<jstring>(), static_cast<jint>(importance))
{
}

QJniObject NotificationChannel::jniObject() const { return m_channel; }

}  // namespace android
