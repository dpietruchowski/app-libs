#include <gtest/gtest.h>

#include "agent/claudecli.h"
#include "chat/application/agentturn.h"
#include "chat/ui/errortext.h"

using chat::errorText;
using chat::MessageStatus;

namespace
{

const QString kLoginCommand = QStringLiteral("env CLAUDE_CONFIG_DIR=/data/chat claude auth login");

QString failedWith(const QString& token)
{
    return errorText(MessageStatus::Failed, token, kLoginCommand);
}

}

TEST(ErrorTextTest, ARunningOrCompleteMessage_HasNoError)
{
    EXPECT_TRUE(errorText(MessageStatus::Running, QString(), kLoginCommand).isEmpty());
    EXPECT_TRUE(errorText(MessageStatus::Complete, QString(), kLoginCommand).isEmpty());
}

TEST(ErrorTextTest, NotLoggedIn_ShowsTheLoginCommand)
{
    const QString text = failedWith(claudecli::kNotLoggedIn);

    EXPECT_TRUE(text.startsWith(QStringLiteral("Not logged in to Claude.")));
    EXPECT_TRUE(text.endsWith(kLoginCommand));
}

TEST(ErrorTextTest, NetworkError_SaysThereIsNoConnection)
{
    EXPECT_EQ(failedWith(claudecli::kNetworkError),
              QStringLiteral("No connection to the provider."));
}

TEST(ErrorTextTest, ServerError_SaysTheProviderFailed)
{
    EXPECT_EQ(failedWith(claudecli::kServerError),
              QStringLiteral("The provider failed to answer."));
}

TEST(ErrorTextTest, Timeout_SaysTheRequestTimedOut)
{
    EXPECT_EQ(failedWith(claudecli::kTimeout), QStringLiteral("The request timed out."));
}

TEST(ErrorTextTest, Unavailable_SaysClaudeIsNotAvailable)
{
    EXPECT_EQ(failedWith(chat::turn_error::kUnavailable),
              QStringLiteral("Claude is not available on this device."));
}

TEST(ErrorTextTest, ACancelledMessage_SaysCancelled)
{
    EXPECT_EQ(errorText(MessageStatus::Cancelled, chat::turn_error::kCancelled, kLoginCommand),
              QStringLiteral("Cancelled."));
}

TEST(ErrorTextTest, AnInterruptedMessage_SaysTheAppClosed)
{
    EXPECT_EQ(errorText(MessageStatus::Interrupted, QString(), kLoginCommand),
              QStringLiteral("The answer was interrupted when the app closed."));
}

TEST(ErrorTextTest, AnUnknownToken_ShowsTheGenericText)
{
    EXPECT_EQ(failedWith(QStringLiteral("unknown_error")), QStringLiteral("The request failed."));
}

TEST(ErrorTextTest, TheKeyErrorsOfTheOldHttpClient_AreNoLongerKnown)
{
    EXPECT_EQ(failedWith(QStringLiteral("invalid_key")), QStringLiteral("The request failed."));
    EXPECT_EQ(failedWith(QStringLiteral("insufficient_credits")),
              QStringLiteral("The request failed."));
}
