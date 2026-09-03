#pragma once

#include <QString>

class AndroidNotificationScheduler final
{
public:
    static void scheduleAt(qint64 triggerAtMsecsSinceEpoch, int notificationId,
                           const QString& channelId, const QString& channelName,
                           const QString& title, const QString& text,
                           const QString& smallIcon = "ic_notification");

    static void scheduleDailyAt(int hour, int minute, int notificationId, const QString& channelId,
                                const QString& channelName, const QString& title,
                                const QString& text, const QString& smallIcon = "ic_notification");

    static void cancel(int notificationId);
};
