#include <QSqlDatabase>
#include <QSqlQuery>
#include <gtest/gtest.h>

#include "dbtoolkit/query/pragma.h"

class PragmaTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        QSqlDatabase::removeDatabase("pragma_test_db");
        m_db = QSqlDatabase::addDatabase("QSQLITE", "pragma_test_db");
        m_db.setDatabaseName(":memory:");
        ASSERT_TRUE(m_db.open());
    }

    void TearDown() override { m_db.close(); }

    QSqlDatabase m_db;
};

TEST_F(PragmaTest, ToSql_PlainPragma_GeneratesCorrectSQL)
{
    Pragma pragma("user_version");

    EXPECT_EQ(pragma.toSql(), "PRAGMA user_version");
}

TEST_F(PragmaTest, ToSql_WithArgument_GeneratesCorrectSQL)
{
    Pragma pragma = Pragma("table_info").withArgument("user_settings");

    EXPECT_EQ(pragma.toSql(), "PRAGMA table_info(user_settings)");
}

TEST_F(PragmaTest, ToSql_SetIntValue_GeneratesCorrectSQL)
{
    Pragma pragma = Pragma("user_version").set(5);

    EXPECT_EQ(pragma.toSql(), "PRAGMA user_version = 5");
}

TEST_F(PragmaTest, ToSql_SetBoolValue_GeneratesOnOff)
{
    EXPECT_EQ(Pragma("foreign_keys").set(true).toSql(), "PRAGMA foreign_keys = ON");
    EXPECT_EQ(Pragma("foreign_keys").set(false).toSql(), "PRAGMA foreign_keys = OFF");
}

TEST_F(PragmaTest, ToSql_SetStringValue_GeneratesUnquotedToken)
{
    Pragma pragma = Pragma("journal_mode").set(QStringLiteral("WAL"));

    EXPECT_EQ(pragma.toSql(), "PRAGMA journal_mode = WAL");
}

TEST_F(PragmaTest, Execute_SetUserVersion_PersistsValue)
{
    QVariant result = Pragma("user_version").set(7).execute(m_db);

    EXPECT_EQ(result.toInt(), 1);

    QSqlQuery query(m_db);
    ASSERT_TRUE(query.exec("PRAGMA user_version"));
    ASSERT_TRUE(query.next());
    EXPECT_EQ(query.value(0).toInt(), 7);
}

TEST_F(PragmaTest, Query_UserVersion_ReturnsRow)
{
    QSqlQuery setup(m_db);
    ASSERT_TRUE(setup.exec("PRAGMA user_version = 3"));

    QVector<QVariantMap> rows = Pragma("user_version").query(m_db);

    ASSERT_EQ(rows.size(), 1);
    EXPECT_EQ(rows.first().value("user_version").toInt(), 3);
}

TEST_F(PragmaTest, Query_TableInfo_ReturnsColumns)
{
    QSqlQuery setup(m_db);
    ASSERT_TRUE(setup.exec("CREATE TABLE test_table (id INTEGER PRIMARY KEY, name TEXT)"));

    QVector<QVariantMap> rows = Pragma("table_info").withArgument("test_table").query(m_db);

    ASSERT_EQ(rows.size(), 2);
    EXPECT_EQ(rows.at(0).value("name").toString(), "id");
    EXPECT_EQ(rows.at(1).value("name").toString(), "name");
}

TEST_F(PragmaTest, Execute_ForeignKeysOn_EnforcesConstraints)
{
    EXPECT_EQ(Pragma("foreign_keys").set(true).execute(m_db).toInt(), 1);

    QSqlQuery setup(m_db);
    ASSERT_TRUE(setup.exec("CREATE TABLE parent (id INTEGER PRIMARY KEY)"));
    ASSERT_TRUE(setup.exec(
        "CREATE TABLE child (id INTEGER PRIMARY KEY, parent_id INTEGER REFERENCES parent(id))"));

    QSqlQuery insert(m_db);
    insert.prepare("INSERT INTO child (parent_id) VALUES (?)");
    insert.addBindValue(1);
    EXPECT_FALSE(insert.exec());
}
