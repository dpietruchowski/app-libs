#pragma once

#include <QDateTime>
#include <QString>

// Local notifications scheduled to appear later, even when the app is not
// running. On Android an alarm fires a broadcast that the app's own
// BroadcastReceiver (named with setReceiverClass) turns into a notification;
// the whole broadcast carries the content, so nothing of the app has to run.
// `launchAction` travels as the "launch_action" extra, for the receiver to put
// on the intent that opens the app when the notification is tapped.
// Platforms without a backend do nothing.
struct ScheduledNotification
{
    int id { 0 };
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
};
