#include "androidnotificationscheduler.h"

#include "jni/alarmmanager.h"
#include "jni/context.h"
#include "jni/intent.h"
#include "jni/notificationmanager.h"
#include "jni/pendingintent.h"
#include "platform/notificationscheduler.h"

#include <QCoreApplication>
#include <QtCore/private/qandroidextras_p.h>

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

    const int displayId = notification.displayId != 0 ? notification.displayId : notification.id;
    auto intent = receiverIntent(notification.id)
                      .putExtra(QStringLiteral("display_id"), displayId)
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

void AndroidNotificationScheduler::dismiss(int displayId)
{
    android::NotificationManager::instance().cancel(displayId);
}

void AndroidNotificationScheduler::requestPermission()
{
    if (QNativeInterface::QAndroidApplication::sdkVersion() < 33)
        return;

    const QString permission = QStringLiteral("android.permission.POST_NOTIFICATIONS");
    if (QtAndroidPrivate::checkPermission(permission).result() == QtAndroidPrivate::Authorized)
        return;
    QtAndroidPrivate::requestPermission(permission);
}
