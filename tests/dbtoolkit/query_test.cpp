#include <gtest/gtest.h>

#include "dbtoolkit/query/delete.h"
#include "dbtoolkit/query/insert.h"
#include "dbtoolkit/query/order.h"
#include "dbtoolkit/query/select.h"
#include "dbtoolkit/query/update.h"
#include "dbtoolkit/query/where.h"

class InsertQueryTest : public ::testing::Test
{
};

TEST_F(InsertQueryTest, SingleInsert_GeneratesCorrectSQL)
{
    QVariantMap item;
    item["name"] = "John";
    item["age"] = 30;
    item["email"] = "john@example.com";

    Insert insert;
    insert.into("users").values(item);

    QString sql = insert.toSql();

    EXPECT_TRUE(sql.contains("INSERT INTO users"));
    EXPECT_TRUE(sql.contains("name"));
    EXPECT_TRUE(sql.contains("age"));
    EXPECT_TRUE(sql.contains("email"));
    EXPECT_TRUE(sql.contains("VALUES"));
    EXPECT_EQ(sql.count("?"), 3);
}

TEST_F(InsertQueryTest, SingleInsert_WithColumns_GeneratesCorrectSQL)
{
    QVariantMap item;
    item["name"] = "John";
    item["age"] = 30;

    Insert insert;
    insert.into("users").columns({ "name", "age" }).values(item);

    QString sql = insert.toSql();

    EXPECT_TRUE(sql.contains("INSERT INTO users (name, age) VALUES (?, ?)"));
}

TEST_F(InsertQueryTest, BatchInsert_ThreeItems_GeneratesCorrectSQL)
{
    QVector<QVariantMap> items;

    QVariantMap item1;
    item1["name"] = "John";
    item1["age"] = 30;
    items.append(item1);

    QVariantMap item2;
    item2["name"] = "Jane";
    item2["age"] = 25;
    items.append(item2);

    QVariantMap item3;
    item3["name"] = "Bob";
    item3["age"] = 35;
    items.append(item3);

    Insert insert;
    insert.into("users").batchValues(items);

    QString sql = insert.toSql();

    EXPECT_TRUE(sql.contains("INSERT INTO users"));
    EXPECT_TRUE(sql.contains("VALUES"));
    EXPECT_EQ(sql.count("?"), 6);
    EXPECT_EQ(sql.count("(?, ?)"), 3);
}

TEST_F(InsertQueryTest, BatchInsert_WithColumns_GeneratesCorrectSQL)
{
    QVector<QVariantMap> items;

    QVariantMap item1;
    item1["name"] = "John";
    item1["age"] = 30;
    item1["email"] = "john@example.com";
    items.append(item1);

    QVariantMap item2;
    item2["name"] = "Jane";
    item2["age"] = 25;
    item2["email"] = "jane@example.com";
    items.append(item2);

    Insert insert;
    insert.into("users").columns({ "name", "age" }).batchValues(items);

    QString sql = insert.toSql();

    EXPECT_TRUE(sql.contains("INSERT INTO users (name, age)"));
    EXPECT_TRUE(sql.contains("VALUES (?, ?), (?, ?)"));
    EXPECT_EQ(sql.count("?"), 4);
    EXPECT_EQ(sql.count("(?, ?)"), 2);
    EXPECT_FALSE(sql.contains("email"));

    QString expected = "INSERT INTO users (name, age) VALUES (?, ?), (?, ?)";
    EXPECT_EQ(sql, expected);
}

TEST_F(InsertQueryTest, Value_BuildsSingleInsert)
{
    Insert insert;
    insert.into("users").value("name", "John").value("age", 30);

    QString sql = insert.toSql();

    EXPECT_TRUE(sql.contains("INSERT INTO users"));
    EXPECT_TRUE(sql.contains("name"));
    EXPECT_TRUE(sql.contains("age"));
    EXPECT_EQ(sql.count("?"), 2);
}

TEST_F(InsertQueryTest, EmptyTable_ReturnsEmptySQL)
{
    Insert insert;
    insert.values(QVariantMap { { "name", "John" } });

    QString sql = insert.toSql();

    EXPECT_TRUE(sql.isEmpty());
}

TEST_F(InsertQueryTest, EmptyValues_ReturnsEmptySQL)
{
    Insert insert;
    insert.into("users");

    QString sql = insert.toSql();

    EXPECT_TRUE(sql.isEmpty());
}

class SelectQueryTest : public ::testing::Test
{
};

TEST_F(SelectQueryTest, SimpleSelect_GeneratesCorrectSQL)
{
    Select select({ "name", "age" });
    select.from("users");

    QString sql = select.toSql();

    EXPECT_TRUE(sql.contains("SELECT name, age"));
    EXPECT_TRUE(sql.contains("FROM users"));
}

TEST_F(SelectQueryTest, SelectWithWhere_GeneratesCorrectSQL)
{
    Select select({ "*" });
    select.from("users").where(Where("age").greaterThan(18));

    QString sql = select.toSql();

    EXPECT_TRUE(sql.contains("SELECT *"));
    EXPECT_TRUE(sql.contains("FROM users"));
    EXPECT_TRUE(sql.contains("WHERE"));
    EXPECT_TRUE(sql.contains("age > 18"));
}

TEST_F(SelectQueryTest, SelectWithOrderBy_GeneratesCorrectSQL)
{
    Select select({ "*" });
    select.from("users").orderBy(Order("name").asc());

    QString sql = select.toSql();

    EXPECT_TRUE(sql.contains("ORDER BY name ASC"));
}

TEST_F(SelectQueryTest, SelectWithLimit_GeneratesCorrectSQL)
{
    Select select({ "*" });
    select.from("users").limit(10);

    QString sql = select.toSql();

    EXPECT_TRUE(sql.contains("LIMIT 10"));
}

TEST_F(SelectQueryTest, SelectWithOffset_GeneratesCorrectSQL)
{
    Select select({ "*" });
    select.from("users").limit(10).offset(5);

    QString sql = select.toSql();

    EXPECT_TRUE(sql.contains("LIMIT 10"));
    EXPECT_TRUE(sql.contains("OFFSET 5"));
}

class UpdateQueryTest : public ::testing::Test
{
};

TEST_F(UpdateQueryTest, SimpleUpdate_GeneratesCorrectSQL)
{
    Update update;
    update.table("users").set("name", "John").where(Where("id").equals(1));

    QString sql = update.toSql();

    EXPECT_TRUE(sql.contains("UPDATE users"));
    EXPECT_TRUE(sql.contains("SET name = "));
    EXPECT_TRUE(sql.contains("WHERE"));
    EXPECT_TRUE(sql.contains("id = 1"));
}

TEST_F(UpdateQueryTest, MultipleSet_GeneratesCorrectSQL)
{
    Update update;
    update.table("users").set("name", "John").set("age", 30).where(Where("id").equals(1));

    QString sql = update.toSql();

    EXPECT_TRUE(sql.contains("UPDATE users"));
    EXPECT_TRUE(sql.contains("SET"));
    EXPECT_TRUE(sql.contains("name = "));
    EXPECT_TRUE(sql.contains("age = "));
}

class DeleteQueryTest : public ::testing::Test
{
};

TEST_F(DeleteQueryTest, DeleteWithWhere_GeneratesCorrectSQL)
{
    Delete del;
    del.from("users").where(Where("id").equals(1));

    QString sql = del.toSql();

    EXPECT_TRUE(sql.contains("DELETE FROM users"));
    EXPECT_TRUE(sql.contains("WHERE"));
    EXPECT_TRUE(sql.contains("id = 1"));
}

TEST_F(DeleteQueryTest, DeleteAll_GeneratesCorrectSQL)
{
    Delete del;
    del.from("users").all();

    QString sql = del.toSql();

    EXPECT_TRUE(sql.contains("DELETE FROM users"));
    EXPECT_FALSE(sql.contains("WHERE"));
}

class WhereQueryTest : public ::testing::Test
{
};

TEST_F(WhereQueryTest, Equals_GeneratesCorrectSQL)
{
    Where where("name");
    where.equals("John");

    QString sql = where.build();

    EXPECT_TRUE(sql.contains("name = 'John'"));
}

TEST_F(WhereQueryTest, GreaterThan_GeneratesCorrectSQL)
{
    Where where("age");
    where.greaterThan(18);

    QString sql = where.build();

    EXPECT_TRUE(sql.contains("age > 18"));
}

TEST_F(WhereQueryTest, LessThan_GeneratesCorrectSQL)
{
    Where where("age");
    where.lessThan(65);

    QString sql = where.build();

    EXPECT_TRUE(sql.contains("age < 65"));
}

TEST_F(WhereQueryTest, And_GeneratesCorrectSQL)
{
    Where where("age");
    where.greaterThan(18).and_("status").equals("active");

    QString sql = where.build();

    EXPECT_TRUE(sql.contains("age > 18"));
    EXPECT_TRUE(sql.contains("AND"));
    EXPECT_TRUE(sql.contains("status = 'active'"));
}

TEST_F(WhereQueryTest, Or_GeneratesCorrectSQL)
{
    Where where("status");
    where.equals("active").or_("status").equals("pending");

    QString sql = where.build();

    EXPECT_TRUE(sql.contains("status = 'active'"));
    EXPECT_TRUE(sql.contains("OR"));
    EXPECT_TRUE(sql.contains("status = 'pending'"));
}

TEST_F(WhereQueryTest, In_GeneratesCorrectSQL)
{
    Where where("id");
    where.in(QList<int> { 1, 2, 3 });

    QString sql = where.build();

    EXPECT_TRUE(sql.contains("id IN (1, 2, 3)"));
}

class OrderQueryTest : public ::testing::Test
{
};

TEST_F(OrderQueryTest, Ascending_GeneratesCorrectSQL)
{
    Order order("name");
    order.asc();

    QString sql = order.build();

    EXPECT_EQ(sql, "name ASC");
}

TEST_F(OrderQueryTest, Descending_GeneratesCorrectSQL)
{
    Order order("created_at");
    order.desc();

    QString sql = order.build();

    EXPECT_EQ(sql, "created_at DESC");
}

TEST_F(OrderQueryTest, MultipleColumns_GeneratesCorrectSQL)
{
    Order order("age");
    order.desc().then("name").asc();

    QString sql = order.build();

    EXPECT_TRUE(sql.contains("age DESC"));
    EXPECT_TRUE(sql.contains("name ASC"));
    EXPECT_TRUE(sql.contains(","));
}
