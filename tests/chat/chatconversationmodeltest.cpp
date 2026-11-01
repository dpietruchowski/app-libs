#include <gtest/gtest.h>

#include <QSignalSpy>
#include <QUuid>
#include <vector>

#include "chat/ui/chatconversationmodel.h"
#include "chatfixture.h"

using chat::ChatConversationModel;
using chat::Conversation;

namespace
{

Conversation conversation(const QString& title, int minutesAfterNow = 0)
{
    return Conversation(QUuid::createUuid(), Conversation::Data { title, std::nullopt },
                        chat_test::kNow, chat_test::kNow.addSecs(minutesAfterNow * 60));
}

}

class ChatConversationModelTest : public ::testing::Test
{
protected:
    QVariant value(int row, ChatConversationModel::Roles role) const
    {
        return model.data(model.index(row), role);
    }

    ChatConversationModel model;
};

TEST_F(ChatConversationModelTest, ANewModel_IsEmpty) { EXPECT_EQ(model.rowCount(), 0); }

TEST_F(ChatConversationModelTest, Reset_ExposesEachConversationWithItsRoles)
{
    const Conversation first = conversation(QStringLiteral("Optics"), 5);
    const Conversation second = conversation(QStringLiteral("Light"));

    model.reset({ first, second });

    ASSERT_EQ(model.rowCount(), 2);
    EXPECT_EQ(value(0, ChatConversationModel::ConversationIdRole).toString(),
              first.id().toString(QUuid::WithoutBraces));
    EXPECT_EQ(value(0, ChatConversationModel::TitleRole).toString(), QStringLiteral("Optics"));
    EXPECT_EQ(value(0, ChatConversationModel::CreatedAtRole).toDateTime(), chat_test::kNow);
    EXPECT_EQ(value(0, ChatConversationModel::UpdatedAtRole).toDateTime(),
              chat_test::kNow.addSecs(300));
    EXPECT_EQ(value(1, ChatConversationModel::TitleRole).toString(), QStringLiteral("Light"));
}

TEST_F(ChatConversationModelTest, Reset_ReplacesThePreviousConversationsAndEmitsAModelReset)
{
    model.reset({ conversation(QStringLiteral("old")) });
    QSignalSpy resets(&model, &QAbstractItemModel::modelReset);

    model.reset({ conversation(QStringLiteral("new one")), conversation(QStringLiteral("two")) });

    EXPECT_EQ(resets.count(), 1);
    ASSERT_EQ(model.rowCount(), 2);
    EXPECT_EQ(value(0, ChatConversationModel::TitleRole).toString(), QStringLiteral("new one"));
}

TEST_F(ChatConversationModelTest, Reset_WithTheSameConversations_EmitsNothing)
{
    const std::vector<Conversation> conversations { conversation(QStringLiteral("a")),
                                                    conversation(QStringLiteral("b")) };
    model.reset(conversations);
    QSignalSpy resets(&model, &QAbstractItemModel::modelReset);

    model.reset(conversations);

    EXPECT_EQ(resets.count(), 0);
    EXPECT_EQ(model.rowCount(), 2);
}

TEST_F(ChatConversationModelTest, Reset_ToNoConversations_EmptiesTheModel)
{
    model.reset({ conversation(QStringLiteral("a")) });

    model.reset({});

    EXPECT_EQ(model.rowCount(), 0);
}

TEST_F(ChatConversationModelTest, ARowOutsideTheModel_HasNoData)
{
    model.reset({ conversation(QStringLiteral("a")) });

    EXPECT_FALSE(value(1, ChatConversationModel::TitleRole).isValid());
    EXPECT_FALSE(model.data(QModelIndex(), ChatConversationModel::TitleRole).isValid());
}

TEST_F(ChatConversationModelTest, TheRoleNames_AreTheOnesTheViewsRead)
{
    const QHash<int, QByteArray> names = model.roleNames();

    EXPECT_EQ(names.value(ChatConversationModel::ConversationIdRole), QByteArray("conversationId"));
    EXPECT_EQ(names.value(ChatConversationModel::TitleRole), QByteArray("title"));
    EXPECT_EQ(names.value(ChatConversationModel::CreatedAtRole), QByteArray("createdAt"));
    EXPECT_EQ(names.value(ChatConversationModel::UpdatedAtRole), QByteArray("updatedAt"));
}
