#include <QSqlDatabase>
#include <QSqlQuery>
#include <gtest/gtest.h>
#include <memory>

#include "dbtoolkit/dbstorage.h"
#include "dbtoolkit/transaction.h"

class TransactionTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        database = QSqlDatabase::addDatabase("QSQLITE", "transaction_test_connection");
        database.setDatabaseName(":memory:");
        ASSERT_TRUE(database.open());

        QSqlQuery query(database);
        ASSERT_TRUE(query.exec("CREATE TABLE items (name TEXT NOT NULL)"));

        storage = std::make_unique<DbStorage>(database);
    }

    void TearDown() override
    {
        storage.reset();
        database.close();
        database = QSqlDatabase();
        QSqlDatabase::removeDatabase("transaction_test_connection");
    }

    void insertItem(const QString& name)
    {
        QSqlQuery query(database);
        query.prepare("INSERT INTO items (name) VALUES (?)");
        query.addBindValue(name);
        ASSERT_TRUE(query.exec());
    }

    int itemCount()
    {
        QSqlQuery query(database);
        query.exec("SELECT COUNT(*) FROM items");
        query.next();
        return query.value(0).toInt();
    }

    QSqlDatabase database;
    std::unique_ptr<DbStorage> storage;
};

TEST_F(TransactionTest, Commit_PersistsWrites)
{
    {
        Transaction transaction(*storage);
        ASSERT_TRUE(transaction.isActive());
        insertItem("a");
        EXPECT_TRUE(transaction.commit());
    }

    EXPECT_EQ(itemCount(), 1);
    EXPECT_EQ(storage->transactionDepth(), 0);
}

TEST_F(TransactionTest, ScopeExitWithoutCommit_RollsBack)
{
    {
        Transaction transaction(*storage);
        insertItem("a");
    }

    EXPECT_EQ(itemCount(), 0);
    EXPECT_EQ(storage->transactionDepth(), 0);
}

TEST_F(TransactionTest, CommitTwice_SecondIsRejected)
{
    Transaction transaction(*storage);
    insertItem("a");

    EXPECT_TRUE(transaction.commit());
    EXPECT_FALSE(transaction.commit());
    EXPECT_FALSE(transaction.isActive());
}

TEST_F(TransactionTest, NestedRollback_LeavesOuterWritesIntact)
{
    Transaction outer(*storage);
    insertItem("outer");
    {
        Transaction inner(*storage);
        insertItem("inner");
        EXPECT_EQ(storage->transactionDepth(), 2);
    }

    EXPECT_EQ(storage->transactionDepth(), 1);
    EXPECT_EQ(itemCount(), 1);
    EXPECT_TRUE(outer.commit());
    EXPECT_EQ(itemCount(), 1);
}

TEST_F(TransactionTest, NestedCommit_ThenOuterRollback_DiscardsBoth)
{
    {
        Transaction outer(*storage);
        insertItem("outer");
        {
            Transaction inner(*storage);
            insertItem("inner");
            EXPECT_TRUE(inner.commit());
        }
        EXPECT_EQ(itemCount(), 2);
    }

    EXPECT_EQ(itemCount(), 0);
    EXPECT_EQ(storage->transactionDepth(), 0);
}

TEST_F(TransactionTest, NestedCommit_ThenOuterCommit_PersistsBoth)
{
    {
        Transaction outer(*storage);
        insertItem("outer");
        {
            Transaction inner(*storage);
            insertItem("inner");
            EXPECT_TRUE(inner.commit());
        }
        EXPECT_TRUE(outer.commit());
    }

    EXPECT_EQ(itemCount(), 2);
}

TEST_F(TransactionTest, ThreeLevels_MiddleRollbackDropsOnlyItsWrites)
{
    Transaction first(*storage);
    insertItem("first");
    {
        Transaction second(*storage);
        insertItem("second");
        {
            Transaction third(*storage);
            insertItem("third");
            EXPECT_TRUE(third.commit());
        }
        EXPECT_EQ(storage->transactionDepth(), 2);
    }

    EXPECT_EQ(itemCount(), 1);
    EXPECT_TRUE(first.commit());
    EXPECT_EQ(itemCount(), 1);
    EXPECT_EQ(storage->transactionDepth(), 0);
}

TEST_F(TransactionTest, CommitWithoutTransaction_IsRejected)
{
    EXPECT_FALSE(storage->commit());
    EXPECT_FALSE(storage->rollback());
    EXPECT_EQ(storage->transactionDepth(), 0);
}
