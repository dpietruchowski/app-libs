#include "notificationscheduler.h"

#include <QtGlobal>

#if defined(Q_OS_ANDROID)
#include "android/androidnotificationscheduler.h"
#define NOTIFICATION_SCHEDULER_BACKEND AndroidNotificationScheduler
#endif

void NotificationScheduler::setReceiverClass(const QString& className)
{
#ifdef NOTIFICATION_SCHEDULER_BACKEND
    NOTIFICATION_SCHEDULER_BACKEND::setReceiverClass(className);
#else
    Q_UNUSED(className);
#endif
}

bool NotificationScheduler::isAvailable()
{
#ifdef NOTIFICATION_SCHEDULER_BACKEND
    return NOTIFICATION_SCHEDULER_BACKEND::isAvailable();
#else
    return false;
#endif
}

void NotificationScheduler::schedule(const ScheduledNotification& notification)
{
#ifdef NOTIFICATION_SCHEDULER_BACKEND
    NOTIFICATION_SCHEDULER_BACKEND::schedule(notification);
#else
    Q_UNUSED(notification);
#endif
}

void NotificationScheduler::cancel(int notificationId)
{
#ifdef NOTIFICATION_SCHEDULER_BACKEND
    NOTIFICATION_SCHEDULER_BACKEND::cancel(notificationId);
#else
    Q_UNUSED(notificationId);
#endif
}

void NotificationScheduler::dismiss(int displayId)
{
#ifdef NOTIFICATION_SCHEDULER_BACKEND
    NOTIFICATION_SCHEDULER_BACKEND::dismiss(displayId);
#else
    Q_UNUSED(displayId);
#endif
}

void NotificationScheduler::requestPermission()
{
#ifdef NOTIFICATION_SCHEDULER_BACKEND
    NOTIFICATION_SCHEDULER_BACKEND::requestPermission();
#endif
}
