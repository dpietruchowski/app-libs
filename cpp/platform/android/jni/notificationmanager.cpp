#include "notificationmanager.h"

#include "context.h"
#include "notification.h"
#include "notificationchannel.h"

namespace android
{

NotificationManager NotificationManager::instance()
{
    return NotificationManager(Context::application().systemService("notification"));
}

NotificationManager::NotificationManager(QJniObject jni)
    : m_manager(std::move(jni))
{
}

void NotificationManager::createChannel(const NotificationChannel& channel) const
{
    m_manager.callMethod<void>("createNotificationChannel", "(Landroid/app/NotificationChannel;)V",
                               channel.jniObject().object<jobject>());
}

void NotificationManager::notify(int id, const Notification& notification) const
{
    m_manager.callMethod<void>("notify", "(ILandroid/app/Notification;)V", static_cast<jint>(id),
                               notification.jniObject().object<jobject>());
}

void NotificationManager::cancel(int id) const
{
    m_manager.callMethod<void>("cancel", "(I)V", static_cast<jint>(id));
}

}  // namespace android
