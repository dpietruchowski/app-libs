#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QSignalSpy>

#include "chat/application/unavailablebackend.h"

class UnavailableBackendTest : public ::testing::Test
{
protected:
    chat::UnavailableBackend backend;
    chat::AgentCatalog catalog;
};

TEST_F(UnavailableBackendTest, Available_IsFalse) { EXPECT_FALSE(backend.available()); }

TEST_F(UnavailableBackendTest, Start_FailsWithUnavailableOnceTheEventLoopRuns)
{
    const std::unique_ptr<chat::AgentTurn> turn = backend.start(chat::TurnRequest {}, catalog);
    QSignalSpy failed(turn.get(), &chat::AgentTurn::failed);
    QSignalSpy finished(turn.get(), &chat::AgentTurn::finished);

    EXPECT_TRUE(failed.isEmpty());

    QCoreApplication::processEvents();

    ASSERT_EQ(failed.count(), 1);
    EXPECT_EQ(failed.at(0).at(0).toString(), chat::turn_error::kUnavailable);
    EXPECT_TRUE(finished.isEmpty());
}

TEST_F(UnavailableBackendTest, Start_CancelBeforeTheEventLoopRuns_StillReportsUnavailableOnce)
{
    const std::unique_ptr<chat::AgentTurn> turn = backend.start(chat::TurnRequest {}, catalog);
    QSignalSpy failed(turn.get(), &chat::AgentTurn::failed);

    turn->cancel();
    QCoreApplication::processEvents();

    ASSERT_EQ(failed.count(), 1);
    EXPECT_EQ(failed.at(0).at(0).toString(), chat::turn_error::kUnavailable);
}

TEST_F(UnavailableBackendTest, Start_EachCallReturnsAnIndependentTurn)
{
    const std::unique_ptr<chat::AgentTurn> first = backend.start(chat::TurnRequest {}, catalog);
    const std::unique_ptr<chat::AgentTurn> second = backend.start(chat::TurnRequest {}, catalog);
    QSignalSpy firstFailed(first.get(), &chat::AgentTurn::failed);
    QSignalSpy secondFailed(second.get(), &chat::AgentTurn::failed);

    QCoreApplication::processEvents();

    EXPECT_NE(first.get(), second.get());
    EXPECT_EQ(firstFailed.count(), 1);
    EXPECT_EQ(secondFailed.count(), 1);
}
