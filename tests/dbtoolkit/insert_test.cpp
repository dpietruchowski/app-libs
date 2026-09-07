#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <gtest/gtest.h>

#include "dbtoolkit/query/insert.h"

class InsertTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        database = QSqlDatabase::addDatabase("QSQLITE", "insert_test_connection");
        database.setDatabaseName(":memory:");
        ASSERT_TRUE(database.open());

        QSqlQuery query(database);
        ASSERT_TRUE(query.exec("CREATE TABLE scores (player TEXT NOT NULL, level INTEGER NOT NULL, "
                               "points INTEGER NOT NULL, PRIMARY KEY (player, level))"));
    }

    void TearDown() override
    {
        database.close();
        database = QSqlDatabase();
        QSqlDatabase::removeDatabase("insert_test_connection");
    }

    QVariantMap score(const QString& player, int level, int points)
    {
        return { { "player", player }, { "level", level }, { "points", points } };
    }

    int pointsOf(const QString& player, int level)
    {
        QSqlQuery query(database);
        query.prepare("SELECT points FROM scores WHERE player = ? AND level = ?");
        query.addBindValue(player);
        query.addBindValue(level);
        query.exec();
        return query.next() ? query.value(0).toInt() : -1;
    }

    int rowCount()
    {
        QSqlQuery query(database);
        query.exec("SELECT COUNT(*) FROM scores");
        query.next();
        return query.value(0).toInt();
    }

    QSqlDatabase database;
};

TEST_F(InsertTest, WithoutOnConflict_EmitsPlainInsert)
{
    Insert insert;
    insert.into("scores").columns({ "player", "level", "points" }).values(score("ann", 1, 10));

    EXPECT_EQ(insert.toSql(), "INSERT INTO scores (player, level, points) VALUES (?, ?, ?)");
}

TEST_F(InsertTest, OnConflict_SingleColumn_UpdatesEveryOtherColumn)
{
    Insert insert;
    insert.into("scores")
        .columns({ "player", "level", "points" })
        .values(score("ann", 1, 10))
        .onConflict({ "player" });

    EXPECT_EQ(insert.toSql(),
              "INSERT INTO scores (player, level, points) VALUES (?, ?, ?) "
              "ON CONFLICT(player) DO UPDATE SET level = excluded.level, points = excluded.points");
}

TEST_F(InsertTest, OnConflict_CompositeKey_UpdatesRemainingColumns)
{
    Insert insert;
    insert.into("scores")
        .columns({ "player", "level", "points" })
        .values(score("ann", 1, 10))
        .onConflict({ "player", "level" });

    EXPECT_EQ(insert.toSql(),
              "INSERT INTO scores (player, level, points) VALUES (?, ?, ?) "
              "ON CONFLICT(player, level) DO UPDATE SET points = excluded.points");
}

TEST_F(InsertTest, OnConflict_OnlyKeyColumns_DoesNothing)
{
    Insert insert;
    insert.into("tags").columns({ "fact_id", "tag" }).values({ { "fact_id", 1 }, { "tag", "x" } });
    insert.onConflict({ "fact_id", "tag" });

    EXPECT_EQ(insert.toSql(),
              "INSERT INTO tags (fact_id, tag) VALUES (?, ?) ON CONFLICT(fact_id, tag) DO NOTHING");
}

TEST_F(InsertTest, OnConflict_ExistingCompositeKey_UpdatesInsteadOfFailing)
{
    Insert first;
    first.into("scores").columns({ "player", "level", "points" }).values(score("ann", 1, 10));
    ASSERT_TRUE(first.execute(database).isValid());

    Insert second;
    second.into("scores")
        .columns({ "player", "level", "points" })
        .values(score("ann", 1, 25))
        .onConflict({ "player", "level" });

    EXPECT_TRUE(second.execute(database).isValid());
    EXPECT_EQ(rowCount(), 1);
    EXPECT_EQ(pointsOf("ann", 1), 25);
}

TEST_F(InsertTest, OnConflict_Batch_InsertsNewAndUpdatesExisting)
{
    Insert seed;
    seed.into("scores")
        .columns({ "player", "level", "points" })
        .batchValues({ score("ann", 1, 10), score("bob", 1, 20) });
    ASSERT_TRUE(seed.execute(database).isValid());

    Insert batch;
    batch.into("scores")
        .columns({ "player", "level", "points" })
        .batchValues({ score("ann", 1, 11), score("bob", 2, 22), score("cid", 1, 33) })
        .onConflict({ "player", "level" });

    EXPECT_TRUE(batch.execute(database).isValid());
    EXPECT_EQ(rowCount(), 4);
    EXPECT_EQ(pointsOf("ann", 1), 11);
    EXPECT_EQ(pointsOf("bob", 1), 20);
    EXPECT_EQ(pointsOf("bob", 2), 22);
    EXPECT_EQ(pointsOf("cid", 1), 33);
}

TEST_F(InsertTest, OnConflict_UpdateBeforeAnyInsertOnTheConnection_StillSucceeds)
{
    QTemporaryDir dir;
    const QString path = dir.path() + "/scores.db";
    {
        QSqlDatabase seeded = QSqlDatabase::addDatabase("QSQLITE", "insert_test_seeded");
        seeded.setDatabaseName(path);
        ASSERT_TRUE(seeded.open());
        QSqlQuery seed(seeded);
        ASSERT_TRUE(seed.exec("CREATE TABLE scores (player TEXT NOT NULL, level INTEGER NOT NULL, "
                              "points INTEGER NOT NULL, PRIMARY KEY (player, level))"));
        ASSERT_TRUE(seed.exec("INSERT INTO scores (player, level, points) VALUES ('ann', 1, 10)"));
        seeded.close();
    }
    QSqlDatabase::removeDatabase("insert_test_seeded");

    QSqlDatabase fresh = QSqlDatabase::addDatabase("QSQLITE", "insert_test_fresh");
    fresh.setDatabaseName(path);
    ASSERT_TRUE(fresh.open());

    Insert update;
    update.into("scores")
        .columns({ "player", "level", "points" })
        .values(score("ann", 1, 25))
        .onConflict({ "player", "level" });
    EXPECT_TRUE(update.execute(fresh).isValid());

    QSqlQuery check(fresh);
    ASSERT_TRUE(check.exec("SELECT points FROM scores WHERE player = 'ann' AND level = 1"));
    ASSERT_TRUE(check.next());
    EXPECT_EQ(check.value(0).toInt(), 25);
    check.finish();
    fresh.close();
    fresh = QSqlDatabase();
    QSqlDatabase::removeDatabase("insert_test_fresh");
}

TEST_F(InsertTest, WithoutOnConflict_DuplicateKey_Fails)
{
    Insert first;
    first.into("scores").columns({ "player", "level", "points" }).values(score("ann", 1, 10));
    ASSERT_TRUE(first.execute(database).isValid());

    Insert duplicate;
    duplicate.into("scores").columns({ "player", "level", "points" }).values(score("ann", 1, 25));

    EXPECT_FALSE(duplicate.execute(database).isValid());
    EXPECT_EQ(pointsOf("ann", 1), 10);
}
