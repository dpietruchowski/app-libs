#include "androidnotification.h"

#include "jni/context.h"
#include "jni/notification.h"
#include "jni/notificationchannel.h"
#include "jni/notificationmanager.h"
#include "jni/resources.h"

namespace
{

int toImportanceInt(AndroidNotification::Importance importance)
{
    switch (importance)
    {
        case AndroidNotification::Importance::Low:
            return 2;
        case AndroidNotification::Importance::Default:
            return 3;
        case AndroidNotification::Importance::High:
            return 4;
        case AndroidNotification::Importance::Max:
            return 5;
    }
    return 3;
}

int resolveDrawableId(const QString& drawableName)
{
    auto context = android::Context::application();
    return context.resources().identifier(drawableName, "drawable", context.packageName());
}

}  // namespace

void AndroidNotification::registerChannel(const QString& channelId, const QString& channelName,
                                          Importance importance)
{
    android::NotificationManager::instance().createChannel(
        android::NotificationChannel(channelId, channelName, toImportanceInt(importance)));
}

AndroidNotification::AndroidNotification(int id, const QString& channelId)
    : m_id(id)
    , m_channelId(channelId)
{
}

AndroidNotification& AndroidNotification::setTitle(const QString& title)
{
    m_title = title;
    return *this;
}

AndroidNotification& AndroidNotification::setText(const QString& text)
{
    m_text = text;
    return *this;
}

AndroidNotification& AndroidNotification::setSmallIcon(const QString& drawableName)
{
    m_drawableName = drawableName;
    return *this;
}

AndroidNotification& AndroidNotification::setAutoCancel(bool autoCancel)
{
    m_autoCancel = autoCancel;
    return *this;
}

void AndroidNotification::show() const
{
    auto context = android::Context::application();
    android::Notification notification = android::Notification::Builder(context, m_channelId)
                                             .setContentTitle(m_title)
                                             .setContentText(m_text)
                                             .setSmallIcon(resolveDrawableId(m_drawableName))
                                             .setAutoCancel(m_autoCancel)
                                             .build();

    android::NotificationManager::instance().notify(m_id, notification);
}
