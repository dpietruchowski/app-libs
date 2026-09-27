#pragma once

#include <QString>

struct ScheduledNotification;

class AndroidNotificationScheduler final
{
public:
    static void setReceiverClass(const QString& className);
    static bool isAvailable();
    static void schedule(const ScheduledNotification& notification);
    static void cancel(int notificationId);
};
