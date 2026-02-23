#include "androidnotificationscheduler.h"

#include "jni/alarmmanager.h"
#include "jni/context.h"
#include "jni/intent.h"
#include "jni/pendingintent.h"

#include <QDateTime>

namespace
{

android::Intent buildReceiverIntent(int notificationId, const QString& channelId,
                                    const QString& channelName, const QString& title,
                                    const QString& text, const QString& smallIcon)
{
    return android::Intent("com.fillin.app.NOTIFICATION")
        .setClassName("com.fillin.app", "com.fillin.app.NotificationReceiver")
        .putExtra("notification_id", notificationId)
        .putExtra("channel_id", channelId)
        .putExtra("channel_name", channelName)
        .putExtra("title", title)
        .putExtra("text", text)
        .putExtra("small_icon", smallIcon);
}

qint64 nextTriggerMillis(int hour, int minute)
{
    QDateTime now = QDateTime::currentDateTime();
    QDateTime trigger = now;
    trigger.setTime(QTime(hour, minute, 0));
    if (trigger <= now)
        trigger = trigger.addDays(1);
    return trigger.toMSecsSinceEpoch();
}

}  // namespace

void AndroidNotificationScheduler::scheduleAt(qint64 triggerAtMsecsSinceEpoch, int notificationId,
                                              const QString& channelId, const QString& channelName,
                                              const QString& title, const QString& text,
                                              const QString& smallIcon)
{
    auto intent
        = buildReceiverIntent(notificationId, channelId, channelName, title, text, smallIcon);
    auto pendingIntent = android::PendingIntent::getBroadcast(notificationId, intent);
    android::AlarmManager::instance().setExactAndAllowWhileIdle(triggerAtMsecsSinceEpoch,
                                                                pendingIntent);
}

void AndroidNotificationScheduler::scheduleDailyAt(int hour, int minute, int notificationId,
                                                   const QString& channelId,
                                                   const QString& channelName, const QString& title,
                                                   const QString& text, const QString& smallIcon)
{
    auto intent
        = buildReceiverIntent(notificationId, channelId, channelName, title, text, smallIcon);
    auto pendingIntent = android::PendingIntent::getBroadcast(notificationId, intent);
    android::AlarmManager::instance().setExactAndAllowWhileIdle(nextTriggerMillis(hour, minute),
                                                                pendingIntent);
}

void AndroidNotificationScheduler::cancel(int notificationId)
{
    auto intent = android::Intent("com.fillin.app.NOTIFICATION");
    auto pendingIntent = android::PendingIntent::getBroadcast(notificationId, intent);
    QJniObject alarmManager = android::Context::application().systemService("alarm");
    alarmManager.callMethod<void>("cancel", "(Landroid/app/PendingIntent;)V",
                                  pendingIntent.jniObject().object<jobject>());
}
