#pragma once

#include <QDateTime>
#include <QString>

// Local notifications scheduled to appear later, even when the app is not
// running. On Android an alarm fires a broadcast that the app's own
// BroadcastReceiver (named with setReceiverClass) turns into a notification;
// the whole broadcast carries the content, so nothing of the app has to run.
// `launchAction` travels as the "launch_action" extra, for the receiver to put
// on the intent that opens the app when the notification is tapped.
// `id` names the alarm; `displayId` names the notification it shows (0 = same
// as `id`), so several alarms can share one slot in the shade, each replacing
// the previous one. `dismiss` removes a shown notification by that slot.
// Platforms without a backend do nothing.
struct ScheduledNotification
{
    int id { 0 };
    int displayId { 0 };
    QDateTime fireAt;
    QString channelId;
    QString channelName;
    QString title;
    QString text;
    QString smallIcon { QStringLiteral("ic_notification") };
    QString launchAction;
};

class NotificationScheduler final
{
public:
    static void setReceiverClass(const QString& className);
    static bool isAvailable();
    static void schedule(const ScheduledNotification& notification);
    static void cancel(int notificationId);
    static void dismiss(int displayId);
    static void requestPermission();
};
