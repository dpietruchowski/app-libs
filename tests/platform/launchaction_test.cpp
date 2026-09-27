#include "platform/launchaction.h"

#include <gtest/gtest.h>

TEST(LaunchActionTest, WithoutBackend_NothingIsPending)
{
    LaunchAction launchAction;

    EXPECT_TRUE(launchAction.take().isEmpty());
}

TEST(LaunchActionTest, DeliveredAction_IsAnnouncedAndTakenOnce)
{
    LaunchAction launchAction;
    QStringList received;
    QObject::connect(&launchAction, &LaunchAction::received,
                     [&received](const QString& action) { received << action; });

    launchAction.deliver(QStringLiteral("review"));

    EXPECT_EQ(received, QStringList { QStringLiteral("review") });
    EXPECT_EQ(launchAction.take(), QStringLiteral("review"));
    EXPECT_TRUE(launchAction.take().isEmpty());
}

TEST(LaunchActionTest, EmptyAction_IsIgnored)
{
    LaunchAction launchAction;
    int received = 0;
    QObject::connect(&launchAction, &LaunchAction::received, [&received] { ++received; });

    launchAction.deliver(QString());

    EXPECT_EQ(received, 0);
    EXPECT_TRUE(launchAction.take().isEmpty());
}
