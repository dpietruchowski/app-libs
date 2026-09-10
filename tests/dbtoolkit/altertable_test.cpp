#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <gtest/gtest.h>

#include "dbtoolkit/query/altertable.h"

class AlterTableTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        QSqlDatabase::removeDatabase("altertable_test_db");
        m_db = QSqlDatabase::addDatabase("QSQLITE", "altertable_test_db");
        m_db.setDatabaseName(":memory:");
        ASSERT_TRUE(m_db.open());

        QSqlQuery query(m_db);
        ASSERT_TRUE(query.exec("CREATE TABLE attempts (uuid TEXT, exercise_type INTEGER)"));
    }

    void TearDown() override { m_db.close(); }

    bool hasColumn(const QString& name) { return m_db.record("attempts").contains(name); }

    QSqlDatabase m_db;
};

TEST_F(AlterTableTest, ToSql_RenameColumn_GeneratesCorrectSQL)
{
    AlterTable alter("attempts");
    alter.renameColumn("exercise_type", "mode");

    EXPECT_EQ(alter.toSql(), "ALTER TABLE attempts RENAME COLUMN exercise_type TO mode");
}

TEST_F(AlterTableTest, RenameColumn_MovesTheColumnAndItsValues)
{
    QSqlQuery insert(m_db);
    ASSERT_TRUE(insert.exec("INSERT INTO attempts VALUES ('a', 7)"));

    AlterTable alter("attempts");
    alter.renameColumn("exercise_type", "mode");
    ASSERT_EQ(alter.execute(m_db).toInt(), 1);

    EXPECT_FALSE(hasColumn("exercise_type"));
    EXPECT_TRUE(hasColumn("mode"));

    QSqlQuery read(m_db);
    ASSERT_TRUE(read.exec("SELECT mode FROM attempts"));
    ASSERT_TRUE(read.next());
    EXPECT_EQ(read.value(0).toInt(), 7);
}

TEST_F(AlterTableTest, RenameColumn_RunTwice_IsANoOpTheSecondTime)
{
    AlterTable alter("attempts");
    alter.renameColumn("exercise_type", "mode");

    ASSERT_EQ(alter.execute(m_db).toInt(), 1);
    EXPECT_EQ(alter.execute(m_db).toInt(), 1);

    EXPECT_TRUE(hasColumn("mode"));
}

TEST_F(AlterTableTest, RenameColumn_TargetAlreadyPresent_LeavesBothColumnsAlone)
{
    QSqlQuery query(m_db);
    ASSERT_TRUE(query.exec("ALTER TABLE attempts ADD COLUMN mode INTEGER"));

    AlterTable alter("attempts");
    alter.renameColumn("exercise_type", "mode");
    ASSERT_EQ(alter.execute(m_db).toInt(), 1);

    EXPECT_TRUE(hasColumn("exercise_type"));
    EXPECT_TRUE(hasColumn("mode"));
}

TEST_F(AlterTableTest, RenameColumn_CarriesAnIndexOverToTheNewName)
{
    QSqlQuery query(m_db);
    ASSERT_TRUE(query.exec("CREATE INDEX attempts_by_type ON attempts (exercise_type)"));

    AlterTable alter("attempts");
    alter.renameColumn("exercise_type", "mode");
    ASSERT_EQ(alter.execute(m_db).toInt(), 1);

    QSqlQuery definition(m_db);
    ASSERT_TRUE(definition.exec(
        "SELECT sql FROM sqlite_master WHERE type='index' AND name='attempts_by_type'"));
    ASSERT_TRUE(definition.next());
    EXPECT_TRUE(definition.value(0).toString().contains("mode"));
}
