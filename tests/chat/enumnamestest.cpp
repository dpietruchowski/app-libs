#include <gtest/gtest.h>

#include "chat/domain/enumnames.h"

using namespace chat;

TEST(EnumNamesTest, Roles_AreNamedAsTheAgentNamesThem)
{
    EXPECT_EQ(roleName(Role::User), QStringLiteral("user"));
    EXPECT_EQ(roleName(Role::Assistant), QStringLiteral("assistant"));
}

TEST(EnumNamesTest, MessageStatuses_AreLowerCase)
{
    EXPECT_EQ(statusName(MessageStatus::Running), QStringLiteral("running"));
    EXPECT_EQ(statusName(MessageStatus::Complete), QStringLiteral("complete"));
    EXPECT_EQ(statusName(MessageStatus::Failed), QStringLiteral("failed"));
    EXPECT_EQ(statusName(MessageStatus::Cancelled), QStringLiteral("cancelled"));
    EXPECT_EQ(statusName(MessageStatus::Interrupted), QStringLiteral("interrupted"));
}

TEST(EnumNamesTest, StepKinds_AreLowerCase)
{
    EXPECT_EQ(stepKindName(StepKind::Delegation), QStringLiteral("delegation"));
    EXPECT_EQ(stepKindName(StepKind::Tool), QStringLiteral("tool"));
}

TEST(EnumNamesTest, StepStatuses_AreLowerCase)
{
    EXPECT_EQ(stepStatusName(StepStatus::Running), QStringLiteral("running"));
    EXPECT_EQ(stepStatusName(StepStatus::Ok), QStringLiteral("ok"));
    EXPECT_EQ(stepStatusName(StepStatus::Failed), QStringLiteral("failed"));
}

TEST(EnumNamesTest, ToolEffects_AreLowerCase)
{
    EXPECT_EQ(effectName(ToolEffect::Reads), QStringLiteral("reads"));
    EXPECT_EQ(effectName(ToolEffect::Writes), QStringLiteral("writes"));
}
