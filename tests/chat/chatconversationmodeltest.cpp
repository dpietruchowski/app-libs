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
    struct RowSignals final
    {
        explicit RowSignals(const ChatConversationModel& model)
            : resets(&model, &QAbstractItemModel::modelReset)
            , inserts(&model, &QAbstractItemModel::rowsInserted)
            , removals(&model, &QAbstractItemModel::rowsRemoved)
            , moves(&model, &QAbstractItemModel::rowsMoved)
            , changes(&model, &QAbstractItemModel::dataChanged)
        {
        }

        QSignalSpy resets;
        QSignalSpy inserts;
        QSignalSpy removals;
        QSignalSpy moves;
        QSignalSpy changes;
    };

    QVariant value(int row, ChatConversationModel::Roles role) const
    {
        return model.data(model.index(row), role);
    }

    QStringList ids() const
    {
        QStringList result;
        for (int row = 0; row < model.rowCount(); ++row)
        {
            result.append(value(row, ChatConversationModel::ConversationIdRole).toString());
        }
        return result;
    }

    static QStringList idsOf(const std::vector<Conversation>& conversations)
    {
        QStringList result;
        for (const Conversation& conversation : conversations)
        {
            result.append(conversation.id().toString(QUuid::WithoutBraces));
        }
        return result;
    }

    static Conversation withTitle(const Conversation& source, const QString& title)
    {
        return Conversation(source.id(), Conversation::Data { title, std::nullopt },
                            source.createdAt(), source.updatedAt());
    }

    static Conversation updatedAfter(const Conversation& source, int seconds)
    {
        return Conversation(source.id(), source.data(), source.createdAt(),
                            source.updatedAt().addSecs(seconds));
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

TEST_F(ChatConversationModelTest, Update_OnAnEmptyModel_InsertsTheRowsInOneRange)
{
    const std::vector<Conversation> conversations { conversation(QStringLiteral("a")),
                                                    conversation(QStringLiteral("b")),
                                                    conversation(QStringLiteral("c")) };
    RowSignals seen(model);

    model.update(conversations);

    EXPECT_EQ(ids(), idsOf(conversations));
    ASSERT_EQ(seen.inserts.count(), 1);
    EXPECT_EQ(seen.inserts.first().at(1).toInt(), 0);
    EXPECT_EQ(seen.inserts.first().at(2).toInt(), 2);
    EXPECT_EQ(seen.resets.count(), 0);
}

TEST_F(ChatConversationModelTest, Update_WithTheSameConversations_EmitsNothing)
{
    const std::vector<Conversation> conversations { conversation(QStringLiteral("a")),
                                                    conversation(QStringLiteral("b")) };
    model.update(conversations);
    RowSignals seen(model);

    model.update(conversations);

    EXPECT_EQ(seen.resets.count(), 0);
    EXPECT_EQ(seen.inserts.count(), 0);
    EXPECT_EQ(seen.removals.count(), 0);
    EXPECT_EQ(seen.moves.count(), 0);
    EXPECT_EQ(seen.changes.count(), 0);
}

TEST_F(ChatConversationModelTest, Update_WithARenamedConversation_ChangesOnlyItsTitle)
{
    const Conversation first = conversation(QStringLiteral("a"));
    const Conversation second = conversation(QStringLiteral("b"));
    model.update({ first, second });
    RowSignals seen(model);

    model.update({ first, withTitle(second, QStringLiteral("renamed")) });

    ASSERT_EQ(seen.changes.count(), 1);
    EXPECT_EQ(seen.changes.first().at(0).value<QModelIndex>().row(), 1);
    EXPECT_EQ(seen.changes.first().at(1).value<QModelIndex>().row(), 1);
    EXPECT_EQ(seen.changes.first().at(2).value<QList<int>>(),
              QList<int> { ChatConversationModel::TitleRole });
    EXPECT_EQ(value(1, ChatConversationModel::TitleRole).toString(), QStringLiteral("renamed"));
    EXPECT_EQ(value(0, ChatConversationModel::TitleRole).toString(), QStringLiteral("a"));
    EXPECT_EQ(seen.resets.count(), 0);
    EXPECT_EQ(seen.moves.count(), 0);
}

TEST_F(ChatConversationModelTest, Update_WithANewerUpdateTimeOnTheTopRow_ChangesOnlyItsUpdateTime)
{
    const Conversation first = conversation(QStringLiteral("a"), 10);
    const Conversation second = conversation(QStringLiteral("b"));
    model.update({ first, second });
    RowSignals seen(model);

    model.update({ updatedAfter(first, 60), second });

    ASSERT_EQ(seen.changes.count(), 1);
    EXPECT_EQ(seen.changes.first().at(0).value<QModelIndex>().row(), 0);
    EXPECT_EQ(seen.changes.first().at(2).value<QList<int>>(),
              QList<int> { ChatConversationModel::UpdatedAtRole });
    EXPECT_EQ(value(0, ChatConversationModel::UpdatedAtRole).toDateTime(),
              chat_test::kNow.addSecs(660));
    EXPECT_EQ(seen.resets.count(), 0);
    EXPECT_EQ(seen.moves.count(), 0);
}

TEST_F(ChatConversationModelTest, Update_WhenAConversationBecomesTheNewest_MovesItsRowToTheTop)
{
    const Conversation first = conversation(QStringLiteral("a"), 3);
    const Conversation second = conversation(QStringLiteral("b"), 2);
    const Conversation third = conversation(QStringLiteral("c"), 1);
    model.update({ first, second, third });
    RowSignals seen(model);

    model.update({ updatedAfter(third, 600), first, second });

    ASSERT_EQ(seen.moves.count(), 1);
    EXPECT_EQ(seen.moves.first().at(1).toInt(), 2);
    EXPECT_EQ(seen.moves.first().at(2).toInt(), 2);
    EXPECT_EQ(seen.moves.first().at(4).toInt(), 0);
    ASSERT_EQ(seen.changes.count(), 1);
    EXPECT_EQ(seen.changes.first().at(0).value<QModelIndex>().row(), 0);
    EXPECT_EQ(seen.changes.first().at(2).value<QList<int>>(),
              QList<int> { ChatConversationModel::UpdatedAtRole });
    EXPECT_EQ(ids(), idsOf({ third, first, second }));
    EXPECT_EQ(value(0, ChatConversationModel::TitleRole).toString(), QStringLiteral("c"));
    EXPECT_EQ(seen.resets.count(), 0);
    EXPECT_EQ(seen.inserts.count(), 0);
    EXPECT_EQ(seen.removals.count(), 0);
}

TEST_F(ChatConversationModelTest, Update_WithANewConversation_InsertsOnlyItsRow)
{
    const Conversation first = conversation(QStringLiteral("a"));
    const Conversation second = conversation(QStringLiteral("b"));
    const Conversation added = conversation(QStringLiteral("new"), 5);
    model.update({ first, second });
    RowSignals seen(model);

    model.update({ added, first, second });

    ASSERT_EQ(seen.inserts.count(), 1);
    EXPECT_EQ(seen.inserts.first().at(1).toInt(), 0);
    EXPECT_EQ(seen.inserts.first().at(2).toInt(), 0);
    EXPECT_EQ(ids(), idsOf({ added, first, second }));
    EXPECT_EQ(seen.resets.count(), 0);
    EXPECT_EQ(seen.moves.count(), 0);
    EXPECT_EQ(seen.removals.count(), 0);
    EXPECT_EQ(seen.changes.count(), 0);
}

TEST_F(ChatConversationModelTest, Update_WithADeletedConversation_RemovesOnlyItsRow)
{
    const Conversation first = conversation(QStringLiteral("a"));
    const Conversation second = conversation(QStringLiteral("b"));
    const Conversation third = conversation(QStringLiteral("c"));
    model.update({ first, second, third });
    RowSignals seen(model);

    model.update({ first, third });

    ASSERT_EQ(seen.removals.count(), 1);
    EXPECT_EQ(seen.removals.first().at(1).toInt(), 1);
    EXPECT_EQ(seen.removals.first().at(2).toInt(), 1);
    EXPECT_EQ(ids(), idsOf({ first, third }));
    EXPECT_EQ(seen.resets.count(), 0);
    EXPECT_EQ(seen.moves.count(), 0);
    EXPECT_EQ(seen.inserts.count(), 0);
}

TEST_F(ChatConversationModelTest, Update_ToNoConversations_RemovesEveryRowWithoutAReset)
{
    model.update({ conversation(QStringLiteral("a")), conversation(QStringLiteral("b")) });
    RowSignals seen(model);

    model.update({});

    EXPECT_EQ(model.rowCount(), 0);
    EXPECT_EQ(seen.removals.count(), 2);
    EXPECT_EQ(seen.resets.count(), 0);
}

TEST_F(ChatConversationModelTest, Update_WithRemovalsInsertionsAndMovesAtOnce_ReachesTheNewOrder)
{
    const Conversation a = conversation(QStringLiteral("a"));
    const Conversation b = conversation(QStringLiteral("b"));
    const Conversation c = conversation(QStringLiteral("c"));
    const Conversation d = conversation(QStringLiteral("d"));
    const Conversation e = conversation(QStringLiteral("e"));
    const Conversation f = conversation(QStringLiteral("f"));
    model.update({ a, b, c, d });
    RowSignals seen(model);

    model.update({ e, d, a, f });

    EXPECT_EQ(ids(), idsOf({ e, d, a, f }));
    EXPECT_EQ(value(1, ChatConversationModel::TitleRole).toString(), QStringLiteral("d"));
    EXPECT_EQ(value(2, ChatConversationModel::TitleRole).toString(), QStringLiteral("a"));
    EXPECT_EQ(seen.resets.count(), 0);
}

TEST_F(ChatConversationModelTest, Update_ToTheReversedOrder_ReachesItWithoutAReset)
{
    const std::vector<Conversation> conversations { conversation(QStringLiteral("a")),
                                                    conversation(QStringLiteral("b")),
                                                    conversation(QStringLiteral("c")),
                                                    conversation(QStringLiteral("d")) };
    model.update(conversations);
    RowSignals seen(model);

    model.update(std::vector<Conversation>(conversations.rbegin(), conversations.rend()));

    EXPECT_EQ(ids(),
              idsOf(std::vector<Conversation>(conversations.rbegin(), conversations.rend())));
    EXPECT_EQ(seen.resets.count(), 0);
    EXPECT_GT(seen.moves.count(), 0);
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
