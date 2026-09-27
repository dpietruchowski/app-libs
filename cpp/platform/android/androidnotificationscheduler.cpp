#include "androidnotificationscheduler.h"

#include "jni/alarmmanager.h"
#include "jni/context.h"
#include "jni/intent.h"
#include "jni/pendingintent.h"
#include "platform/notificationscheduler.h"

namespace
{

QString& receiverClass()
{
    static QString className;
    return className;
}

android::Intent receiverIntent(int notificationId)
{
    const QString packageName = android::Context::application().packageName();
    return android::Intent(packageName + QStringLiteral(".NOTIFICATION"))
        .setClassName(packageName, receiverClass())
        .putExtra(QStringLiteral("notification_id"), notificationId);
}

}  // namespace

void AndroidNotificationScheduler::setReceiverClass(const QString& className)
{
    receiverClass() = className;
}

bool AndroidNotificationScheduler::isAvailable() { return !receiverClass().isEmpty(); }

void AndroidNotificationScheduler::schedule(const ScheduledNotification& notification)
{
    if (!isAvailable())
        return;

    auto intent = receiverIntent(notification.id)
                      .putExtra(QStringLiteral("channel_id"), notification.channelId)
                      .putExtra(QStringLiteral("channel_name"), notification.channelName)
                      .putExtra(QStringLiteral("title"), notification.title)
                      .putExtra(QStringLiteral("text"), notification.text)
                      .putExtra(QStringLiteral("small_icon"), notification.smallIcon)
                      .putExtra(QStringLiteral("launch_action"), notification.launchAction);
    auto pendingIntent = android::PendingIntent::getBroadcast(notification.id, intent);
    android::AlarmManager::instance().setAndAllowWhileIdle(notification.fireAt.toMSecsSinceEpoch(),
                                                           pendingIntent);
}

void AndroidNotificationScheduler::cancel(int notificationId)
{
    if (!isAvailable())
        return;

    auto pendingIntent
        = android::PendingIntent::getBroadcast(notificationId, receiverIntent(notificationId));
    android::AlarmManager::instance().cancel(pendingIntent);
}
