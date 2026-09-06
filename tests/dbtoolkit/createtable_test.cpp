#include <QSqlDatabase>
#include <QSqlQuery>
#include <gtest/gtest.h>

#include "dbtoolkit/query/column.h"
#include "dbtoolkit/query/createtable.h"

class CreateTableTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        database = QSqlDatabase::addDatabase("QSQLITE", "createtable_test_connection");
        database.setDatabaseName(":memory:");
        ASSERT_TRUE(database.open());
    }

    void TearDown() override
    {
        database.close();
        database = QSqlDatabase();
        QSqlDatabase::removeDatabase("createtable_test_connection");
    }

    bool insertPair(const QString& table, int a, int b)
    {
        QSqlQuery query(database);
        query.prepare(QString("INSERT INTO %1 (a, b) VALUES (?, ?)").arg(table));
        query.addBindValue(a);
        query.addBindValue(b);
        return query.exec();
    }

    QSqlDatabase database;
};

TEST_F(CreateTableTest, PrimaryKey_EmitsTableConstraintAfterColumns)
{
    CreateTable table("pairs");
    table.column(Column("a").integer().notNull())
        .column(Column("b").integer().notNull())
        .column(Column("value").text())
        .primaryKey({ "a", "b" });

    EXPECT_EQ(table.toSql(),
              "CREATE TABLE pairs (a INTEGER NOT NULL, b INTEGER NOT NULL, value TEXT, "
              "PRIMARY KEY(a, b))");
}

TEST_F(CreateTableTest, PrimaryKey_PrecedesForeignKeys)
{
    CreateTable table("pairs");
    table.column(Column("a").integer().notNull())
        .column(Column("b").integer().notNull())
        .primaryKey({ "a", "b" })
        .foreignKey("a", "parents", "id", OnDeleteAction::Cascade);

    EXPECT_EQ(table.toSql(),
              "CREATE TABLE pairs (a INTEGER NOT NULL, b INTEGER NOT NULL, PRIMARY KEY(a, b), "
              "FOREIGN KEY(a) REFERENCES parents(id) ON DELETE CASCADE)");
}

TEST_F(CreateTableTest, CompositePrimaryKey_RejectsDuplicatePairOnly)
{
    CreateTable table("pairs");
    table.column(Column("a").integer().notNull())
        .column(Column("b").integer().notNull())
        .primaryKey({ "a", "b" });
    ASSERT_EQ(table.execute(database).toInt(), 1);

    EXPECT_TRUE(insertPair("pairs", 1, 1));
    EXPECT_TRUE(insertPair("pairs", 1, 2));
    EXPECT_TRUE(insertPair("pairs", 2, 1));
    EXPECT_FALSE(insertPair("pairs", 1, 1));
}
