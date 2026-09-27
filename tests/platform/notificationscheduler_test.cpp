#include "platform/notificationscheduler.h"

#include <gtest/gtest.h>

TEST(NotificationSchedulerTest, WithoutBackend_IsUnavailableEvenWithReceiver)
{
    NotificationScheduler::setReceiverClass(QStringLiteral("com.example.NotificationReceiver"));

    EXPECT_FALSE(NotificationScheduler::isAvailable());
}

TEST(NotificationSchedulerTest, WithoutBackend_ScheduleAndCancelAreNoOps)
{
    ScheduledNotification notification;
    notification.id = 1001;
    notification.fireAt = QDateTime::currentDateTime().addDays(1);
    notification.title = QStringLiteral("title");

    EXPECT_NO_FATAL_FAILURE(NotificationScheduler::schedule(notification));
    EXPECT_NO_FATAL_FAILURE(NotificationScheduler::cancel(notification.id));
}
