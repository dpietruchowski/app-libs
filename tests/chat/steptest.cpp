#include <gtest/gtest.h>

#include <vector>

#include "chat/domain/step.h"
#include "chatfixture.h"

using chat_test::toolStep;

TEST(StepTest, UpsertStep_AppendsAStepWithANewId)
{
    const chat::Step first = toolStep();
    const chat::Step second = toolStep();
    std::vector<chat::Step> steps { first };

    chat::upsertStep(steps, second);

    ASSERT_EQ(steps.size(), 2u);
    EXPECT_EQ(steps[0], first);
    EXPECT_EQ(steps[1], second);
}

TEST(StepTest, UpsertStep_ReplacesTheStepWithTheSameIdInPlace)
{
    const chat::Step first = toolStep();
    chat::Step second = toolStep();
    std::vector<chat::Step> steps { first, second };
    second.status = chat::StepStatus::Ok;

    chat::upsertStep(steps, second);

    ASSERT_EQ(steps.size(), 2u);
    EXPECT_EQ(steps[0], first);
    EXPECT_EQ(steps[1].status, chat::StepStatus::Ok);
}
