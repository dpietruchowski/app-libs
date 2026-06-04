#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <gtest/gtest.h>

#include "dbtoolkit/query/column.h"
#include "dbtoolkit/query/createindex.h"
#include "dbtoolkit/query/createtable.h"

class CreateIndexTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        QSqlDatabase::removeDatabase("test_db");
        m_db = QSqlDatabase::addDatabase("QSQLITE", "test_db");
        m_db.setDatabaseName(":memory:");
        ASSERT_TRUE(m_db.open());
    }

    void TearDown() override { m_db.close(); }

    QSqlDatabase m_db;
};

TEST_F(CreateIndexTest, PlainIndex_GeneratesCorrectSQL)
{
    CreateIndex idx("test_idx", "test_table");
    idx.columns({ "col1", "col2" });

    QString sql = idx.toSql();

    EXPECT_TRUE(sql.contains("CREATE INDEX test_idx ON test_table (col1, col2)"));
}

TEST_F(CreateIndexTest, UniqueIndex_GeneratesCorrectSQL)
{
    CreateIndex idx("test_idx", "test_table");
    idx.unique().columns({ "col1" });

    QString sql = idx.toSql();

    EXPECT_TRUE(sql.contains("CREATE UNIQUE INDEX test_idx ON test_table (col1)"));
}

TEST_F(CreateIndexTest, IfNotExists_GeneratesCorrectSQL)
{
    CreateIndex idx("test_idx", "test_table");
    idx.ifNotExists().columns({ "col1" });

    QString sql = idx.toSql();

    EXPECT_TRUE(sql.contains("CREATE INDEX IF NOT EXISTS test_idx ON test_table (col1)"));
}

TEST_F(CreateIndexTest, ExpressionBasedColumn_GeneratesCorrectSQL)
{
    CreateIndex idx("test_idx", "test_table");
    idx.columns({ "COALESCE(col1, '')", "col2" });

    QString sql = idx.toSql();

    EXPECT_TRUE(sql.contains("COALESCE(col1, '')"));
    EXPECT_TRUE(sql.contains("col2"));
}

TEST_F(CreateIndexTest, PartialIndex_GeneratesCorrectSQL)
{
    CreateIndex idx("test_idx", "test_table");
    idx.columns({ "col1" }).where(Where("col2").isNotNull());

    QString sql = idx.toSql();

    EXPECT_TRUE(sql.contains("CREATE INDEX test_idx ON test_table (col1)"));
    EXPECT_TRUE(sql.contains("WHERE"));
    EXPECT_TRUE(sql.contains("col2 IS NOT NULL"));
}

TEST_F(CreateIndexTest, UniqueExpressionIndex_GeneratesCorrectSQL)
{
    CreateIndex idx("expressions_unique", "expressions");
    idx.unique().columns({ "text", "COALESCE(part_of_speech, '')", "language" });

    QString sql = idx.toSql();

    EXPECT_TRUE(sql.contains("CREATE UNIQUE INDEX expressions_unique ON expressions"));
    EXPECT_TRUE(sql.contains("COALESCE(part_of_speech, '')"));
}

TEST_F(CreateIndexTest, ExecuteIndex_CreatesIndexInDatabase)
{
    QSqlQuery query(m_db);
    query.exec("CREATE TABLE test_table (id INTEGER PRIMARY KEY, name TEXT, age INTEGER)");

    CreateIndex idx("test_idx", "test_table");
    idx.columns({ "name" });

    QVariant result = idx.execute(m_db);
    EXPECT_EQ(result.toInt(), 1);

    QSqlQuery checkIdx(m_db);
    checkIdx.exec("SELECT name FROM sqlite_master WHERE type='index' AND name='test_idx'");
    EXPECT_TRUE(checkIdx.next());
}

TEST_F(CreateIndexTest, ExecuteUniqueIndex_AcceptsValidData)
{
    QSqlQuery query(m_db);
    query.exec("CREATE TABLE test_table (id INTEGER PRIMARY KEY, value TEXT)");

    CreateIndex idx("test_unique_idx", "test_table");
    idx.unique().columns({ "value" });

    QVariant result = idx.execute(m_db);
    EXPECT_EQ(result.toInt(), 1);

    QSqlQuery insertQuery(m_db);
    insertQuery.prepare("INSERT INTO test_table (value) VALUES (?)");
    insertQuery.addBindValue("unique_val");
    EXPECT_TRUE(insertQuery.exec());

    insertQuery.prepare("INSERT INTO test_table (value) VALUES (?)");
    insertQuery.addBindValue("unique_val");
    EXPECT_FALSE(insertQuery.exec());
}

TEST_F(CreateIndexTest, ExecutePartialIndex_WithWhereClause)
{
    QSqlQuery query(m_db);
    query.exec("CREATE TABLE test_table (id INTEGER PRIMARY KEY, value TEXT, active INTEGER)");

    CreateIndex idx("test_partial_idx", "test_table");
    idx.columns({ "value" }).where(Where("active").isNotNull());

    QVariant result = idx.execute(m_db);
    EXPECT_EQ(result.toInt(), 1);

    QSqlQuery checkIdx(m_db);
    checkIdx.exec("SELECT sql FROM sqlite_master WHERE type='index' AND name='test_partial_idx'");
    EXPECT_TRUE(checkIdx.next());
    QString indexSql = checkIdx.value(0).toString();
    EXPECT_TRUE(indexSql.contains("WHERE"));
}
