#include <gtest/gtest.h>

#include <QDateTime>
#include <QList>
#include <QSignalSpy>
#include <QUuid>
#include <QVariantList>
#include <QVariantMap>
#include <optional>
#include <vector>

#include "chat/ui/chatmessagemodel.h"
#include "chatfixture.h"

using chat::ChatMessageModel;
using chat::MessageStatus;
using chat::Role;
using chat::Step;
using chat::StepKind;
using chat::StepStatus;

namespace
{

const QString kLoginCommand = QStringLiteral("env CLAUDE_CONFIG_DIR=/data/chat claude auth login");

chat::Message message(Role role, const QString& text,
                      MessageStatus status = MessageStatus::Complete,
                      const QString& error = QString(), const std::vector<Step>& steps = {})
{
    chat::Message::Data data;
    data.role = role;
    data.text = text;
    data.status = status;
    data.error = error;
    data.steps = steps;
    return chat::Message::create(data, chat_test::kNow);
}

Step step(StepKind kind, const QString& agent, const QString& tool, StepStatus status,
          ToolEffect effect = ToolEffect::Reads,
          const std::optional<QUuid>& parentId = std::nullopt)
{
    Step result;
    result.id = QUuid::createUuid();
    result.parentId = parentId;
    result.kind = kind;
    result.agent = agent;
    result.tool = tool;
    result.status = status;
    result.effect = effect;
    return result;
}

}

class ChatMessageModelTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        model.setAgentTitles({ { QStringLiteral("courses"), QStringLiteral("Courses") } });
    }

    QVariant value(int row, ChatMessageModel::Roles role) const
    {
        return model.data(model.index(row), role);
    }

    QVariantList stepsAt(int row) const { return value(row, ChatMessageModel::StepsRole).toList(); }

    ChatMessageModel model;
};

TEST_F(ChatMessageModelTest, ANewModel_IsEmpty) { EXPECT_EQ(model.rowCount(), 0); }

TEST_F(ChatMessageModelTest, Reset_ExposesEachMessageWithItsRoles)
{
    model.reset({ message(Role::User, QStringLiteral("hello")),
                  message(Role::Assistant, QStringLiteral("hi there")) });

    ASSERT_EQ(model.rowCount(), 2);
    EXPECT_EQ(value(0, ChatMessageModel::AuthorRole).toString(), QStringLiteral("user"));
    EXPECT_EQ(value(0, ChatMessageModel::TextRole).toString(), QStringLiteral("hello"));
    EXPECT_EQ(value(0, ChatMessageModel::StatusRole).toString(), QStringLiteral("complete"));
    EXPECT_EQ(value(0, ChatMessageModel::CreatedAtRole).toDateTime(), chat_test::kNow);
    EXPECT_EQ(value(1, ChatMessageModel::AuthorRole).toString(), QStringLiteral("assistant"));
    EXPECT_EQ(value(1, ChatMessageModel::TextRole).toString(), QStringLiteral("hi there"));
}

TEST_F(ChatMessageModelTest, Reset_ReplacesThePreviousMessagesAndEmitsAModelReset)
{
    model.reset({ message(Role::User, QStringLiteral("old")) });
    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);

    model.reset({ message(Role::User, QStringLiteral("new one")),
                  message(Role::Assistant, QStringLiteral("new two")) });

    EXPECT_EQ(reset.count(), 1);
    ASSERT_EQ(model.rowCount(), 2);
    EXPECT_EQ(value(0, ChatMessageModel::TextRole).toString(), QStringLiteral("new one"));
}

TEST_F(ChatMessageModelTest, ARowOutsideTheModel_HasNoData)
{
    model.reset({ message(Role::User, QStringLiteral("hello")) });

    EXPECT_FALSE(value(1, ChatMessageModel::TextRole).isValid());
    EXPECT_FALSE(model.data(QModelIndex(), ChatMessageModel::TextRole).isValid());
}

TEST_F(ChatMessageModelTest, TheRoleNames_AreTheOnesTheViewsRead)
{
    const QHash<int, QByteArray> names = model.roleNames();

    EXPECT_EQ(names.value(ChatMessageModel::AuthorRole), QByteArray("role"));
    EXPECT_EQ(names.value(ChatMessageModel::TextRole), QByteArray("text"));
    EXPECT_EQ(names.value(ChatMessageModel::StatusRole), QByteArray("status"));
    EXPECT_EQ(names.value(ChatMessageModel::ErrorRole), QByteArray("error"));
    EXPECT_EQ(names.value(ChatMessageModel::StepsRole), QByteArray("steps"));
    EXPECT_EQ(names.value(ChatMessageModel::CreatedAtRole), QByteArray("createdAt"));
}

TEST_F(ChatMessageModelTest, ACompleteMessage_HasNoError)
{
    model.reset({ message(Role::Assistant, QStringLiteral("fine")) });

    EXPECT_TRUE(value(0, ChatMessageModel::ErrorRole).toString().isEmpty());
}

TEST_F(ChatMessageModelTest, AFailedMessage_ShowsTheTextOfItsToken)
{
    model.reset({ message(Role::Assistant, QString(), MessageStatus::Failed,
                          QStringLiteral("server_error")) });

    EXPECT_EQ(value(0, ChatMessageModel::StatusRole).toString(), QStringLiteral("failed"));
    EXPECT_EQ(value(0, ChatMessageModel::ErrorRole).toString(),
              QStringLiteral("The provider failed to answer."));
}

TEST_F(ChatMessageModelTest, ANotLoggedInMessage_ShowsTheLoginCommandOfTheModel)
{
    model.setLoginCommand(kLoginCommand);
    model.reset({ message(Role::Assistant, QString(), MessageStatus::Failed,
                          QStringLiteral("not_logged_in")) });

    EXPECT_TRUE(value(0, ChatMessageModel::ErrorRole).toString().endsWith(kLoginCommand));
}

TEST_F(ChatMessageModelTest, SetLoginCommand_RefreshesTheErrorsAlreadyShown)
{
    model.reset({ message(Role::Assistant, QString(), MessageStatus::Failed,
                          QStringLiteral("not_logged_in")) });
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);

    model.setLoginCommand(kLoginCommand);

    ASSERT_EQ(changed.count(), 1);
    EXPECT_EQ(changed.at(0).at(2).value<QList<int>>(), QList<int> { ChatMessageModel::ErrorRole });
    EXPECT_TRUE(value(0, ChatMessageModel::ErrorRole).toString().endsWith(kLoginCommand));
}

TEST_F(ChatMessageModelTest, SetLoginCommand_ToTheSameCommand_EmitsNothing)
{
    model.setLoginCommand(kLoginCommand);
    model.reset({ message(Role::Assistant, QString(), MessageStatus::Failed,
                          QStringLiteral("not_logged_in")) });
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);

    model.setLoginCommand(kLoginCommand);

    EXPECT_EQ(changed.count(), 0);
}

TEST_F(ChatMessageModelTest, Steps_CarryTheTitleOfTheirAgentAndTheirParent)
{
    const Step delegation = step(StepKind::Delegation, QStringLiteral("courses"),
                                 QStringLiteral("ask_courses"), StepStatus::Ok);
    const Step tool
        = step(StepKind::Tool, QStringLiteral("courses"), QStringLiteral("courses_list"),
               StepStatus::Ok, ToolEffect::Writes, delegation.id);
    model.reset({ message(Role::Assistant, QStringLiteral("done"), MessageStatus::Complete,
                          QString(), { delegation, tool }) });

    const QVariantList steps = stepsAt(0);

    ASSERT_EQ(steps.size(), 2);
    const QVariantMap first = steps.at(0).toMap();
    EXPECT_EQ(first.value(QStringLiteral("id")).toString(),
              delegation.id.toString(QUuid::WithoutBraces));
    EXPECT_EQ(first.value(QStringLiteral("parentId")).toString(), QString());
    EXPECT_EQ(first.value(QStringLiteral("kind")).toString(), QStringLiteral("delegation"));
    EXPECT_EQ(first.value(QStringLiteral("agent")).toString(), QStringLiteral("courses"));
    EXPECT_EQ(first.value(QStringLiteral("agentTitle")).toString(), QStringLiteral("Courses"));
    EXPECT_EQ(first.value(QStringLiteral("tool")).toString(), QStringLiteral("ask_courses"));
    EXPECT_EQ(first.value(QStringLiteral("status")).toString(), QStringLiteral("ok"));
    EXPECT_EQ(first.value(QStringLiteral("effect")).toString(), QStringLiteral("reads"));
    const QVariantMap second = steps.at(1).toMap();
    EXPECT_EQ(second.value(QStringLiteral("parentId")).toString(),
              delegation.id.toString(QUuid::WithoutBraces));
    EXPECT_EQ(second.value(QStringLiteral("kind")).toString(), QStringLiteral("tool"));
    EXPECT_EQ(second.value(QStringLiteral("effect")).toString(), QStringLiteral("writes"));
}

TEST_F(ChatMessageModelTest, AStepOfAnAgentOutsideTheCatalog_FallsBackToTheAgentKey)
{
    model.reset({ message(Role::Assistant, QString(), MessageStatus::Complete, QString(),
                          { step(StepKind::Delegation, QStringLiteral("facts"),
                                 QStringLiteral("ask_facts"), StepStatus::Ok) }) });

    EXPECT_EQ(stepsAt(0).at(0).toMap().value(QStringLiteral("agentTitle")).toString(),
              QStringLiteral("facts"));
}

TEST_F(ChatMessageModelTest, AMessageWithoutSteps_HasAnEmptyList)
{
    model.reset({ message(Role::User, QStringLiteral("hello")) });

    EXPECT_TRUE(stepsAt(0).isEmpty());
}

TEST_F(ChatMessageModelTest, SetRunningText_ReplacesTheTextOfTheRunningMessage)
{
    model.reset({ message(Role::User, QStringLiteral("question")),
                  message(Role::Assistant, QString(), MessageStatus::Running) });
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);

    EXPECT_TRUE(model.setRunningText(QStringLiteral("part")));

    EXPECT_EQ(value(1, ChatMessageModel::TextRole).toString(), QStringLiteral("part"));
    EXPECT_EQ(value(0, ChatMessageModel::TextRole).toString(), QStringLiteral("question"));
    ASSERT_EQ(changed.count(), 1);
    EXPECT_EQ(changed.at(0).at(0).toModelIndex().row(), 1);
    EXPECT_EQ(changed.at(0).at(1).toModelIndex().row(), 1);
    EXPECT_EQ(changed.at(0).at(2).value<QList<int>>(), QList<int> { ChatMessageModel::TextRole });
}

TEST_F(ChatMessageModelTest, SetRunningText_WithTheSameText_ChangesNothing)
{
    model.reset({ message(Role::Assistant, QStringLiteral("part"), MessageStatus::Running) });
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);

    EXPECT_FALSE(model.setRunningText(QStringLiteral("part")));

    EXPECT_EQ(changed.count(), 0);
}

TEST_F(ChatMessageModelTest, SetRunningText_WithoutARunningMessage_ChangesNothing)
{
    model.reset({ message(Role::User, QStringLiteral("question")),
                  message(Role::Assistant, QStringLiteral("answer")) });
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);

    EXPECT_FALSE(model.setRunningText(QStringLiteral("part")));

    EXPECT_EQ(changed.count(), 0);
    EXPECT_EQ(value(1, ChatMessageModel::TextRole).toString(), QStringLiteral("answer"));
}

TEST_F(ChatMessageModelTest, PutRunningStep_AppendsAStepToTheRunningMessage)
{
    model.reset({ message(Role::User, QStringLiteral("question")),
                  message(Role::Assistant, QString(), MessageStatus::Running) });
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
    const Step delegation = step(StepKind::Delegation, QStringLiteral("courses"),
                                 QStringLiteral("ask_courses"), StepStatus::Running);

    EXPECT_TRUE(model.putRunningStep(delegation));

    ASSERT_EQ(stepsAt(1).size(), 1);
    EXPECT_EQ(stepsAt(1).at(0).toMap().value(QStringLiteral("status")).toString(),
              QStringLiteral("running"));
    EXPECT_TRUE(stepsAt(0).isEmpty());
    ASSERT_EQ(changed.count(), 1);
    EXPECT_EQ(changed.at(0).at(2).value<QList<int>>(), QList<int> { ChatMessageModel::StepsRole });
}

TEST_F(ChatMessageModelTest, PutRunningStep_WithAKnownId_ReplacesTheStepInPlace)
{
    Step delegation = step(StepKind::Delegation, QStringLiteral("courses"),
                           QStringLiteral("ask_courses"), StepStatus::Running);
    const Step tool
        = step(StepKind::Tool, QStringLiteral("courses"), QStringLiteral("courses_list"),
               StepStatus::Running, ToolEffect::Reads, delegation.id);
    model.reset({ message(Role::Assistant, QString(), MessageStatus::Running, QString(),
                          { delegation, tool }) });
    delegation.status = StepStatus::Ok;

    EXPECT_TRUE(model.putRunningStep(delegation));

    const QVariantList steps = stepsAt(0);
    ASSERT_EQ(steps.size(), 2);
    EXPECT_EQ(steps.at(0).toMap().value(QStringLiteral("status")).toString(), QStringLiteral("ok"));
    EXPECT_EQ(steps.at(1).toMap().value(QStringLiteral("tool")).toString(),
              QStringLiteral("courses_list"));
}

TEST_F(ChatMessageModelTest, PutRunningStep_WithoutARunningMessage_ChangesNothing)
{
    model.reset({ message(Role::Assistant, QStringLiteral("answer")) });
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);

    EXPECT_FALSE(model.putRunningStep(step(StepKind::Delegation, QStringLiteral("courses"),
                                           QStringLiteral("ask_courses"), StepStatus::Running)));

    EXPECT_EQ(changed.count(), 0);
    EXPECT_TRUE(stepsAt(0).isEmpty());
}

TEST_F(ChatMessageModelTest, SetAgentTitles_RefreshesTheStepsOfTheMessages)
{
    model.reset(
        { message(Role::Assistant, QStringLiteral("done"), MessageStatus::Complete, QString(),
                  { step(StepKind::Delegation, QStringLiteral("facts"), QStringLiteral("ask_facts"),
                         StepStatus::Ok) }) });
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);

    model.setAgentTitles({ { QStringLiteral("facts"), QStringLiteral("Facts") } });

    EXPECT_EQ(stepsAt(0).at(0).toMap().value(QStringLiteral("agentTitle")).toString(),
              QStringLiteral("Facts"));
    ASSERT_EQ(changed.count(), 1);
    EXPECT_EQ(changed.at(0).at(2).value<QList<int>>(), QList<int> { ChatMessageModel::StepsRole });
}

TEST_F(ChatMessageModelTest, SetAgentTitles_WithTheSameTitles_ChangesNothing)
{
    model.reset({ message(Role::User, QStringLiteral("hello")) });
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);

    model.setAgentTitles({ { QStringLiteral("courses"), QStringLiteral("Courses") } });

    EXPECT_EQ(changed.count(), 0);
}

TEST_F(ChatMessageModelTest, SetAgentTitles_WithoutMessages_EmitsNothing)
{
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);

    model.setAgentTitles({ { QStringLiteral("facts"), QStringLiteral("Facts") } });

    EXPECT_EQ(changed.count(), 0);
}

TEST_F(ChatMessageModelTest, Activity_WithoutARunningMessage_IsEmpty)
{
    model.reset(
        { message(Role::Assistant, QStringLiteral("answer"), MessageStatus::Complete, QString(),
                  { step(StepKind::Delegation, QStringLiteral("courses"),
                         QStringLiteral("ask_courses"), StepStatus::Running) }) });

    EXPECT_TRUE(model.activity().isEmpty());
}

TEST_F(ChatMessageModelTest, Activity_OfARunningDelegation_NamesTheAgent)
{
    model.reset({ message(Role::Assistant, QString(), MessageStatus::Running, QString(),
                          { step(StepKind::Delegation, QStringLiteral("courses"),
                                 QStringLiteral("ask_courses"), StepStatus::Running) }) });

    EXPECT_EQ(model.activity(), QStringLiteral("Asking Courses…"));
}

TEST_F(ChatMessageModelTest, Activity_OfARunningTool_NamesTheAgentAndTheTool)
{
    model.reset({ message(Role::Assistant, QString(), MessageStatus::Running, QString(),
                          { step(StepKind::Tool, QStringLiteral("courses"),
                                 QStringLiteral("courses_list"), StepStatus::Running) }) });

    EXPECT_EQ(model.activity(), QStringLiteral("Courses: courses_list…"));
}

TEST_F(ChatMessageModelTest, Activity_IsTheLastRunningStep)
{
    const Step delegation = step(StepKind::Delegation, QStringLiteral("courses"),
                                 QStringLiteral("ask_courses"), StepStatus::Running);
    const Step tool
        = step(StepKind::Tool, QStringLiteral("courses"), QStringLiteral("courses_list"),
               StepStatus::Running, ToolEffect::Reads, delegation.id);
    model.reset({ message(Role::Assistant, QString(), MessageStatus::Running, QString(),
                          { delegation, tool }) });

    EXPECT_EQ(model.activity(), QStringLiteral("Courses: courses_list…"));
}

TEST_F(ChatMessageModelTest, Activity_SkipsTheStepsThatAreFinished)
{
    const Step delegation = step(StepKind::Delegation, QStringLiteral("courses"),
                                 QStringLiteral("ask_courses"), StepStatus::Running);
    const Step tool
        = step(StepKind::Tool, QStringLiteral("courses"), QStringLiteral("courses_list"),
               StepStatus::Ok, ToolEffect::Reads, delegation.id);
    model.reset({ message(Role::Assistant, QString(), MessageStatus::Running, QString(),
                          { delegation, tool }) });

    EXPECT_EQ(model.activity(), QStringLiteral("Asking Courses…"));
}

TEST_F(ChatMessageModelTest, Activity_WhenEveryStepIsFinished_IsEmpty)
{
    model.reset({ message(Role::Assistant, QString(), MessageStatus::Running, QString(),
                          { step(StepKind::Delegation, QStringLiteral("courses"),
                                 QStringLiteral("ask_courses"), StepStatus::Ok) }) });

    EXPECT_TRUE(model.activity().isEmpty());
}

TEST_F(ChatMessageModelTest, Activity_OfAnAgentOutsideTheTitles_FallsBackToTheAgentKey)
{
    model.reset({ message(Role::Assistant, QString(), MessageStatus::Running, QString(),
                          { step(StepKind::Delegation, QStringLiteral("facts"),
                                 QStringLiteral("ask_facts"), StepStatus::Running) }) });

    EXPECT_EQ(model.activity(), QStringLiteral("Asking facts…"));
}

TEST_F(ChatMessageModelTest, Activity_FollowsThePutRunningStep)
{
    model.reset({ message(Role::Assistant, QString(), MessageStatus::Running) });
    Step delegation = step(StepKind::Delegation, QStringLiteral("courses"),
                           QStringLiteral("ask_courses"), StepStatus::Running);
    model.putRunningStep(delegation);
    ASSERT_EQ(model.activity(), QStringLiteral("Asking Courses…"));

    delegation.status = StepStatus::Ok;
    model.putRunningStep(delegation);

    EXPECT_TRUE(model.activity().isEmpty());
}
