#include <gtest/gtest.h>

#include <vector>

#include "chat/application/transcript.h"
#include "chatfixture.h"

namespace
{

chat::Message messageAt(int position, int length = 10,
                        chat::MessageStatus status = chat::MessageStatus::Complete)
{
    chat::Message::Data data;
    data.position = position;
    data.role = position % 2 == 0 ? chat::Role::User : chat::Role::Assistant;
    data.text = QString(length, QLatin1Char('x'));
    data.status = status;
    return chat::Message::create(data, chat_test::kNow);
}

std::vector<chat::Message> history(int count, int length = 10)
{
    std::vector<chat::Message> messages;
    for (int position = 0; position < count; ++position)
    {
        messages.push_back(messageAt(position, length));
    }
    return messages;
}

bool startsWithMarker(const chat::TurnTranscript& transcript)
{
    return !transcript.messages.empty()
        && transcript.messages.front().text() == chat::kOmittedMarker
        && transcript.messages.front().id().isNull();
}

}

TEST(TranscriptLimitsTest, SeedLimits_AreHalfOfTheTranscriptLimits)
{
    EXPECT_EQ(chat::kSeedLimits.maxMessages * 2, chat::kTranscriptLimits.maxMessages);
    EXPECT_EQ(chat::kSeedLimits.maxChars * 2, chat::kTranscriptLimits.maxChars);
}

TEST(FitsLimitsTest, EmptyList_Fits) { EXPECT_TRUE(chat::fitsLimits({}, { 3, 10 })); }

TEST(FitsLimitsTest, ExactlyAtBothLimits_Fits)
{
    EXPECT_TRUE(chat::fitsLimits(history(3, 5), { 3, 15 }));
}

TEST(FitsLimitsTest, OneMessageTooMany_DoesNotFit)
{
    EXPECT_FALSE(chat::fitsLimits(history(4, 1), { 3, 1000 }));
}

TEST(FitsLimitsTest, OneCharacterTooMany_DoesNotFit)
{
    EXPECT_FALSE(chat::fitsLimits(history(3, 5), { 3, 14 }));
}

TEST(TrimTranscriptTest, EmptyHistory_GivesAnEmptyTranscript)
{
    const chat::TurnTranscript transcript = chat::trimTranscript({}, { 3, 100 });

    EXPECT_TRUE(transcript.messages.empty());
    EXPECT_EQ(transcript.start, 0);
}

TEST(TrimTranscriptTest, HistoryThatFits_IsKeptWholeWithoutAMarker)
{
    const chat::TurnTranscript transcript = chat::trimTranscript(history(5), { 10, 1000 });

    EXPECT_EQ(transcript.messages.size(), 5u);
    EXPECT_FALSE(startsWithMarker(transcript));
    EXPECT_EQ(transcript.start, 0);
}

TEST(TrimTranscriptTest, TooManyMessages_DropsTheOldestAndPrependsTheMarker)
{
    const chat::TurnTranscript transcript = chat::trimTranscript(history(30), { 20, 100000 });

    ASSERT_EQ(transcript.messages.size(), 21u);
    EXPECT_TRUE(startsWithMarker(transcript));
    EXPECT_EQ(transcript.messages[1].position(), 10);
    EXPECT_EQ(transcript.messages.back().position(), 29);
    EXPECT_EQ(transcript.start, 10);
}

TEST(TrimTranscriptTest, TooManyCharacters_DropsTheOldestUntilTheRestFits)
{
    const chat::TurnTranscript transcript = chat::trimTranscript(history(10, 100), { 40, 350 });

    ASSERT_EQ(transcript.messages.size(), 4u);
    EXPECT_TRUE(startsWithMarker(transcript));
    EXPECT_EQ(transcript.messages[1].position(), 7);
    EXPECT_EQ(transcript.start, 7);
}

TEST(TrimTranscriptTest, TheLastMessageIsKeptEvenWhenItAloneExceedsTheLimit)
{
    std::vector<chat::Message> messages = history(3, 10);
    messages.push_back(messageAt(3, 500));

    const chat::TurnTranscript transcript = chat::trimTranscript(messages, { 40, 100 });

    ASSERT_EQ(transcript.messages.size(), 2u);
    EXPECT_TRUE(startsWithMarker(transcript));
    EXPECT_EQ(transcript.messages.back().position(), 3);
    EXPECT_EQ(transcript.start, 3);
}

TEST(TrimTranscriptTest, ASingleOversizedMessage_NeedsNoMarker)
{
    const chat::TurnTranscript transcript
        = chat::trimTranscript({ messageAt(0, 500) }, { 40, 100 });

    ASSERT_EQ(transcript.messages.size(), 1u);
    EXPECT_FALSE(startsWithMarker(transcript));
}

TEST(TrimTranscriptTest, TheMarkerIsAUserMessageJustBeforeTheFirstKeptOne)
{
    const chat::TurnTranscript transcript = chat::trimTranscript(history(8), { 3, 1000 });

    ASSERT_TRUE(startsWithMarker(transcript));
    EXPECT_EQ(transcript.messages.front().role(), chat::Role::User);
    EXPECT_EQ(transcript.messages.front().position(), transcript.start - 1);
}

TEST(PlanTranscriptTest, NoSession_ShortHistory_SeedsWithTheWholeHistory)
{
    const chat::TurnTranscript plan = chat::planTranscript(history(30), std::nullopt);

    EXPECT_FALSE(plan.resumed);
    EXPECT_EQ(plan.messages.size(), 30u);
    EXPECT_FALSE(startsWithMarker(plan));
    EXPECT_EQ(plan.start, 0);
}

TEST(PlanTranscriptTest, NoSession_HistoryOverTheCap_SeedsWithHalfTheCap)
{
    const chat::TurnTranscript plan = chat::planTranscript(history(50), std::nullopt);

    EXPECT_FALSE(plan.resumed);
    ASSERT_EQ(plan.messages.size(), 21u);
    EXPECT_TRUE(startsWithMarker(plan));
    EXPECT_EQ(plan.start, 30);
}

TEST(PlanTranscriptTest, SessionSeededFromATrimmedHistory_ResumesWithTheWindowBehindAMarker)
{
    const chat::TurnTranscript plan
        = chat::planTranscript(history(30), chat::BackendSession { QStringLiteral("s1"), 10 });

    EXPECT_TRUE(plan.resumed);
    EXPECT_EQ(plan.start, 10);
    ASSERT_EQ(plan.messages.size(), 21u);
    EXPECT_TRUE(startsWithMarker(plan));
    EXPECT_EQ(plan.messages.front().position(), 9);
    EXPECT_EQ(plan.messages[1].position(), 10);
    EXPECT_EQ(plan.messages.back().position(), 29);
}

TEST(PlanTranscriptTest, SessionStartingAtTheFirstMessage_ResumesWithoutAMarker)
{
    const chat::TurnTranscript plan
        = chat::planTranscript(history(30), chat::BackendSession { QStringLiteral("s1"), 0 });

    EXPECT_TRUE(plan.resumed);
    EXPECT_EQ(plan.messages.size(), 30u);
    EXPECT_FALSE(startsWithMarker(plan));
}

TEST(PlanTranscriptTest, SessionStartingAtTheFirstCompleteMessage_ResumesWithoutAMarker)
{
    std::vector<chat::Message> messages;
    messages.push_back(messageAt(0, 10, chat::MessageStatus::Failed));
    messages.push_back(messageAt(1));
    messages.push_back(messageAt(2));

    const chat::TurnTranscript plan
        = chat::planTranscript(messages, chat::BackendSession { QStringLiteral("s1"), 1 });

    EXPECT_TRUE(plan.resumed);
    EXPECT_EQ(plan.messages.size(), 2u);
    EXPECT_FALSE(startsWithMarker(plan));
}

TEST(PlanTranscriptTest, SessionWindowOfExactlyTheCap_StillResumes)
{
    const chat::TurnTranscript plan
        = chat::planTranscript(history(50), chat::BackendSession { QStringLiteral("s1"), 10 });

    EXPECT_TRUE(plan.resumed);
    EXPECT_EQ(plan.messages.size(), 41u);
    EXPECT_TRUE(startsWithMarker(plan));
}

TEST(PlanTranscriptTest, SessionWindowOneMessageOverTheCap_SeedsAfresh)
{
    const chat::TurnTranscript plan
        = chat::planTranscript(history(51), chat::BackendSession { QStringLiteral("s1"), 10 });

    EXPECT_FALSE(plan.resumed);
    ASSERT_EQ(plan.messages.size(), 21u);
    EXPECT_TRUE(startsWithMarker(plan));
    EXPECT_EQ(plan.start, 31);
}

TEST(PlanTranscriptTest, SessionWindowOverTheCharacterCap_SeedsAfreshWithinHalfTheCharacters)
{
    const chat::TurnTranscript plan
        = chat::planTranscript(history(10, 4000), chat::BackendSession { QStringLiteral("s1"), 0 });

    EXPECT_FALSE(plan.resumed);
    ASSERT_EQ(plan.messages.size(), 4u);
    EXPECT_TRUE(startsWithMarker(plan));
    EXPECT_EQ(plan.start, 7);
}

TEST(PlanTranscriptTest, LongHistoryBeforeTheSessionStart_DoesNotPreventResuming)
{
    const chat::TurnTranscript plan
        = chat::planTranscript(history(200), chat::BackendSession { QStringLiteral("s1"), 180 });

    EXPECT_TRUE(plan.resumed);
    EXPECT_EQ(plan.messages.size(), 21u);
    EXPECT_TRUE(startsWithMarker(plan));
}

TEST(PlanTranscriptTest, MessagesThatAreNotComplete_NeverEnterTheTranscript)
{
    std::vector<chat::Message> messages = history(2);
    messages.push_back(messageAt(2, 10, chat::MessageStatus::Failed));
    messages.push_back(messageAt(3, 10));
    messages.push_back(messageAt(4, 10, chat::MessageStatus::Running));

    const chat::TurnTranscript plan = chat::planTranscript(messages, std::nullopt);

    ASSERT_EQ(plan.messages.size(), 3u);
    EXPECT_EQ(plan.messages[0].position(), 0);
    EXPECT_EQ(plan.messages[1].position(), 1);
    EXPECT_EQ(plan.messages[2].position(), 3);
}

TEST(PlanTranscriptTest, ResumedWindow_StartsAtTheSessionStartEvenWhenThatPositionIsNotComplete)
{
    std::vector<chat::Message> messages = history(4);
    messages.push_back(messageAt(4, 10, chat::MessageStatus::Cancelled));
    messages.push_back(messageAt(5, 10));

    const chat::TurnTranscript plan
        = chat::planTranscript(messages, chat::BackendSession { QStringLiteral("s1"), 4 });

    EXPECT_TRUE(plan.resumed);
    EXPECT_EQ(plan.start, 4);
    ASSERT_EQ(plan.messages.size(), 2u);
    EXPECT_TRUE(startsWithMarker(plan));
    EXPECT_EQ(plan.messages.back().position(), 5);
}
