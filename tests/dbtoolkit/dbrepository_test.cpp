#include <QSqlDatabase>
#include <QSqlQuery>
#include <gtest/gtest.h>
#include <memory>

#include "dbtoolkit/dbtoolkit.h"

class DbRepositoryTest : public ::testing::Test
{
protected:
    std::unique_ptr<DbStorage> storage;
    std::unique_ptr<DbRepository> repository;
    QSqlDatabase database;

    void SetUp() override
    {
        database = QSqlDatabase::addDatabase("QSQLITE", "dbrepository_test_connection");
        database.setDatabaseName(":memory:");
        ASSERT_TRUE(database.open());

        storage = std::make_unique<DbStorage>(database);
        repository = std::make_unique<DbRepository>(
            "test_table", "id", QStringList { "id", "name", "age", "email" }, *storage);

        CreateTable table("test_table");
        table.ifNotExists()
            .column(Column("id").integer().primaryKey().autoIncrement())
            .column(Column("name").text().notNull())
            .column(Column("age").integer())
            .column(Column("email").text());

        repository->createTable(table);
    }

    void TearDown() override
    {
        QSqlQuery query(database);
        query.exec("DROP TABLE IF EXISTS test_table");

        repository.reset();
        storage.reset();

        database.close();
        QSqlDatabase::removeDatabase("dbrepository_test_connection");
    }

    QVariantMap createTestItem(const QString& name, int age, const QString& email = "")
    {
        QVariantMap item;
        item["name"] = name;
        item["age"] = age;
        if (!email.isEmpty())
        {
            item["email"] = email;
        }
        return item;
    }
};

TEST_F(DbRepositoryTest, Insert_NewItem_ReturnsValidId)
{
    QVariantMap item = createTestItem("John", 30, "john@example.com");

    QVariant result = repository->insert(item);

    EXPECT_TRUE(result.isValid());
    EXPECT_GT(result.toInt(), 0);
}

TEST_F(DbRepositoryTest, Insert_MultipleItems_ReturnsValidIds)
{
    QVariantMap item1 = createTestItem("John", 30);
    QVariantMap item2 = createTestItem("Jane", 25);

    QVariant result1 = repository->insert(item1);
    QVariant result2 = repository->insert(item2);

    EXPECT_TRUE(result1.isValid());
    EXPECT_TRUE(result2.isValid());
    EXPECT_NE(result1.toInt(), result2.toInt());
}

TEST_F(DbRepositoryTest, Select_AfterInsert_ReturnsItem)
{
    QVariantMap item = createTestItem("John", 30, "john@example.com");
    repository->insert(item);

    QVector<QVariantMap> results = repository->select();

    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0]["name"].toString(), "John");
    EXPECT_EQ(results[0]["age"].toInt(), 30);
    EXPECT_EQ(results[0]["email"].toString(), "john@example.com");
}

TEST_F(DbRepositoryTest, Select_WithWhere_ReturnsFilteredResults)
{
    repository->insert(createTestItem("John", 30));
    repository->insert(createTestItem("Jane", 25));
    repository->insert(createTestItem("Bob", 35));

    QVector<QVariantMap> results = repository->select(Where("age").greaterThan(28));

    EXPECT_EQ(results.size(), 2);
}

TEST_F(DbRepositoryTest, Select_WithOrder_ReturnsSortedResults)
{
    repository->insert(createTestItem("Charlie", 35));
    repository->insert(createTestItem("Alice", 25));
    repository->insert(createTestItem("Bob", 30));

    QVector<QVariantMap> results = repository->select(Where(), Order("name").asc());

    ASSERT_EQ(results.size(), 3);
    EXPECT_EQ(results[0]["name"].toString(), "Alice");
    EXPECT_EQ(results[1]["name"].toString(), "Bob");
    EXPECT_EQ(results[2]["name"].toString(), "Charlie");
}

TEST_F(DbRepositoryTest, Select_WithLimit_ReturnsLimitedResults)
{
    repository->insert(createTestItem("John", 30));
    repository->insert(createTestItem("Jane", 25));
    repository->insert(createTestItem("Bob", 35));

    QVector<QVariantMap> results = repository->select(Where(), Order(), 2);

    EXPECT_EQ(results.size(), 2);
}

TEST_F(DbRepositoryTest, Update_ExistingItem_UpdatesSuccessfully)
{
    QVariantMap item = createTestItem("John", 30);
    QVariant id = repository->insert(item);

    QVariantMap updateData;
    updateData["id"] = id;
    updateData["name"] = "John Updated";
    updateData["age"] = 31;

    int rowsAffected = repository->update(updateData);

    EXPECT_EQ(rowsAffected, 1);

    QVector<QVariantMap> results = repository->select(Where("id").equals(id));
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0]["name"].toString(), "John Updated");
    EXPECT_EQ(results[0]["age"].toInt(), 31);
}

TEST_F(DbRepositoryTest, Upsert_NewItem_InsertsItem)
{
    QVariantMap item = createTestItem("John", 30);
    item["id"] = 999;

    QVariant result = repository->upsert(item);

    EXPECT_TRUE(result.isValid());

    QVector<QVariantMap> results = repository->select(Where("name").equals("John"));
    EXPECT_EQ(results.size(), 1);
}

TEST_F(DbRepositoryTest, Upsert_ExistingItem_UpdatesItem)
{
    QVariantMap item = createTestItem("John", 30);
    QVariant id = repository->insert(item);

    QVariantMap updateData = item;
    updateData["id"] = id;
    updateData["age"] = 31;

    QVariant result = repository->upsert(updateData);

    EXPECT_TRUE(result.isValid());

    QVector<QVariantMap> results = repository->select(Where("id").equals(id));
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0]["age"].toInt(), 31);
}

TEST_F(DbRepositoryTest, Remove_WithWhere_RemovesMatchingItems)
{
    repository->insert(createTestItem("John", 30));
    repository->insert(createTestItem("Jane", 25));
    repository->insert(createTestItem("Bob", 35));

    int removed = repository->remove(Where("age").lessThan(30));

    EXPECT_EQ(removed, 1);

    QVector<QVariantMap> results = repository->select();
    EXPECT_EQ(results.size(), 2);
}

TEST_F(DbRepositoryTest, Exists_ExistingItem_ReturnsTrue)
{
    QVariantMap item = createTestItem("John", 30);
    repository->insert(item);

    bool exists = repository->exists(Where("name").equals("John"));

    EXPECT_TRUE(exists);
}

TEST_F(DbRepositoryTest, Exists_NonExistingItem_ReturnsFalse)
{
    bool exists = repository->exists(Where("name").equals("NonExistent"));

    EXPECT_FALSE(exists);
}

TEST_F(DbRepositoryTest, Count_EmptyTable_ReturnsZero)
{
    int count = repository->count();

    EXPECT_EQ(count, 0);
}

TEST_F(DbRepositoryTest, Count_WithItems_ReturnsCorrectCount)
{
    repository->insert(createTestItem("John", 30));
    repository->insert(createTestItem("Jane", 25));
    repository->insert(createTestItem("Bob", 35));

    int count = repository->count();

    EXPECT_EQ(count, 3);
}

TEST_F(DbRepositoryTest, Count_WithWhere_ReturnsFilteredCount)
{
    repository->insert(createTestItem("John", 30));
    repository->insert(createTestItem("Jane", 25));
    repository->insert(createTestItem("Bob", 35));

    int count = repository->count(Where("age").greaterThan(28));

    EXPECT_EQ(count, 2);
}

TEST_F(DbRepositoryTest, BatchInsert_ThreeItems_InsertsAllSuccessfully)
{
    QVector<QVariantMap> items;
    items.append(createTestItem("John", 30, "john@example.com"));
    items.append(createTestItem("Jane", 25, "jane@example.com"));
    items.append(createTestItem("Bob", 35, "bob@example.com"));

    QVector<QVariant> ids = repository->insert(items);

    EXPECT_EQ(ids.size(), 3);

    int count = repository->count();
    EXPECT_EQ(count, 3);
}

TEST_F(DbRepositoryTest, BatchInsert_EmptyList_ReturnsZero)
{
    QVector<QVariantMap> items;

    QVector<QVariant> ids = repository->insert(items);

    EXPECT_TRUE(ids.isEmpty());
}

TEST_F(DbRepositoryTest, BatchInsert_LargeChunk_InsertsAll)
{
    QVector<QVariantMap> items;
    for (int i = 0; i < 250; ++i)
    {
        items.append(createTestItem(QString("User%1").arg(i), 20 + (i % 50)));
    }

    QVector<QVariant> ids = repository->insert(items, 100);

    EXPECT_EQ(ids.size(), 250);

    int count = repository->count();
    EXPECT_EQ(count, 250);
}

TEST_F(DbRepositoryTest, BatchInsert_WithCustomChunkSize_InsertsAll)
{
    QVector<QVariantMap> items;
    for (int i = 0; i < 150; ++i)
    {
        items.append(createTestItem(QString("User%1").arg(i), 20 + i));
    }

    QVector<QVariant> ids = repository->insert(items, 50);

    EXPECT_EQ(ids.size(), 150);

    int count = repository->count();
    EXPECT_EQ(count, 150);
}

TEST_F(DbRepositoryTest, BatchUpdate_MultipleItems_UpdatesAllSuccessfully)
{
    QVariant id1 = repository->insert(createTestItem("John", 30));
    QVariant id2 = repository->insert(createTestItem("Jane", 25));
    QVariant id3 = repository->insert(createTestItem("Bob", 35));

    QVector<QVariantMap> updates;

    QVariantMap update1;
    update1["id"] = id1;
    update1["name"] = "John Updated";
    update1["age"] = 31;
    updates.append(update1);

    QVariantMap update2;
    update2["id"] = id2;
    update2["name"] = "Jane Updated";
    update2["age"] = 26;
    updates.append(update2);

    QVariantMap update3;
    update3["id"] = id3;
    update3["name"] = "Bob Updated";
    update3["age"] = 36;
    updates.append(update3);

    QVector<QVariant> ids = repository->updateAll(updates);

    EXPECT_EQ(ids.size(), 3);

    QVector<QVariantMap> results = repository->select();
    ASSERT_EQ(results.size(), 3);

    for (const auto& result : results)
    {
        QString name = result["name"].toString();
        EXPECT_TRUE(name.contains("Updated"));
    }
}

TEST_F(DbRepositoryTest, BatchUpdate_EmptyList_ReturnsZero)
{
    QVector<QVariantMap> updates;

    QVector<QVariant> ids = repository->updateAll(updates);

    EXPECT_TRUE(ids.isEmpty());
}

TEST_F(DbRepositoryTest, UpsertAll_MixedNewAndExisting_UpsertsAll)
{
    QVariant id1 = repository->insert(createTestItem("John", 30));
    QVariant id2 = repository->insert(createTestItem("Jane", 25));

    QVector<QVariantMap> items;

    QVariantMap existing1;
    existing1["id"] = id1;
    existing1["name"] = "John Updated";
    existing1["age"] = 31;
    items.append(existing1);

    QVariantMap existing2;
    existing2["id"] = id2;
    existing2["name"] = "Jane Updated";
    existing2["age"] = 26;
    items.append(existing2);

    items.append(createTestItem("Bob", 35));

    QVector<QVariant> ids = repository->upsertAll(items);

    EXPECT_EQ(ids.size(), 3);

    int count = repository->count();
    EXPECT_EQ(count, 3);

    QVector<QVariantMap> results = repository->select(Where("id").equals(id1));
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0]["name"].toString(), "John Updated");
}

TEST_F(DbRepositoryTest, ClearTable_RemovesAllData)
{
    repository->insert(createTestItem("John", 30));
    repository->insert(createTestItem("Jane", 25));
    repository->insert(createTestItem("Bob", 35));

    repository->clearTable();

    int count = repository->count();
    EXPECT_EQ(count, 0);
}

TEST_F(DbRepositoryTest, BatchInsert_EmptyVector_ReturnsEmptyVector)
{
    QVector<QVariantMap> items;

    QVector<QVariant> ids = repository->insert(items);

    EXPECT_TRUE(ids.isEmpty());
}

TEST_F(DbRepositoryTest, BatchInsert_SingleItem_ReturnsOneId)
{
    QVector<QVariantMap> items;
    items.append(createTestItem("John", 30));

    QVector<QVariant> ids = repository->insert(items);

    ASSERT_EQ(ids.size(), 1);
    EXPECT_TRUE(ids[0].isValid());
}

TEST_F(DbRepositoryTest, BatchInsert_MultipleItems_ReturnsAllIds)
{
    QVector<QVariantMap> items;
    items.append(createTestItem("John", 30));
    items.append(createTestItem("Jane", 25));
    items.append(createTestItem("Bob", 35));

    QVector<QVariant> ids = repository->insert(items);

    EXPECT_EQ(ids.size(), 3);
    for (const auto& id : ids)
    {
        EXPECT_TRUE(id.isValid());
    }
}

TEST_F(DbRepositoryTest, BatchInsert_WithChunking_SplitsCorrectly)
{
    QVector<QVariantMap> items;
    for (int i = 0; i < 350; ++i)
    {
        items.append(createTestItem(QString("User%1").arg(i), 20 + (i % 50)));
    }

    QVector<QVariant> ids = repository->insert(items, 100);

    EXPECT_EQ(ids.size(), 350);
    EXPECT_EQ(repository->count(), 350);
}

TEST_F(DbRepositoryTest, BatchInsert_SmallChunkSize_HandlesCorrectly)
{
    QVector<QVariantMap> items;
    for (int i = 0; i < 25; ++i)
    {
        items.append(createTestItem(QString("User%1").arg(i), 20 + i));
    }

    QVector<QVariant> ids = repository->insert(items, 10);

    EXPECT_EQ(ids.size(), 25);
    EXPECT_EQ(repository->count(), 25);
}

TEST_F(DbRepositoryTest, BatchInsert_ChunkSizeLargerThanItems_InsertsAll)
{
    QVector<QVariantMap> items;
    for (int i = 0; i < 10; ++i)
    {
        items.append(createTestItem(QString("User%1").arg(i), 20 + i));
    }

    QVector<QVariant> ids = repository->insert(items, 100);

    EXPECT_EQ(ids.size(), 10);
    EXPECT_EQ(repository->count(), 10);
}

TEST_F(DbRepositoryTest, BatchExists_EmptyVector_ReturnsEmptySet)
{
    QVector<QVariantMap> items;

    auto existingIds = repository->select();

    EXPECT_TRUE(existingIds.isEmpty());
}

TEST_F(DbRepositoryTest, BatchExists_NoItemsExist_ReturnsEmptySet)
{
    QVector<QVariantMap> items;
    QVariantMap item1;
    item1["id"] = 999;
    item1["name"] = "Test";
    items.append(item1);

    QVariantMap item2;
    item2["id"] = 998;
    item2["name"] = "Test2";
    items.append(item2);

    int count = repository->count(Where("id").in(QList<int> { 999, 998 }));

    EXPECT_EQ(count, 0);
}

TEST_F(DbRepositoryTest, BatchExists_AllItemsExist_ReturnsAllIds)
{
    QVariant id1 = repository->insert(createTestItem("John", 30));
    QVariant id2 = repository->insert(createTestItem("Jane", 25));
    QVariant id3 = repository->insert(createTestItem("Bob", 35));

    QVector<QVariantMap> items;
    QVariantMap item1;
    item1["id"] = id1;
    items.append(item1);

    QVariantMap item2;
    item2["id"] = id2;
    items.append(item2);

    QVariantMap item3;
    item3["id"] = id3;
    items.append(item3);

    int count
        = repository->count(Where("id").in(QList<int> { id1.toInt(), id2.toInt(), id3.toInt() }));

    EXPECT_EQ(count, 3);
}

TEST_F(DbRepositoryTest, BatchExists_MixedExistence_ReturnsOnlyExisting)
{
    QVariant id1 = repository->insert(createTestItem("John", 30));
    QVariant id2 = repository->insert(createTestItem("Jane", 25));

    QVector<QVariantMap> items;
    QVariantMap existing1;
    existing1["id"] = id1;
    items.append(existing1);

    QVariantMap existing2;
    existing2["id"] = id2;
    items.append(existing2);

    QVariantMap nonExisting;
    nonExisting["id"] = 999;
    items.append(nonExisting);

    int count = repository->count(Where("id").in(QList<int> { id1.toInt(), id2.toInt(), 999 }));

    EXPECT_EQ(count, 2);
}

TEST_F(DbRepositoryTest, BatchUpsert_AllNew_InsertsAll)
{
    QVector<QVariantMap> items;
    items.append(createTestItem("John", 30));
    items.append(createTestItem("Jane", 25));
    items.append(createTestItem("Bob", 35));

    QVector<QVariant> ids = repository->upsert(items);

    EXPECT_EQ(ids.size(), 3);
    EXPECT_EQ(repository->count(), 3);
}

TEST_F(DbRepositoryTest, BatchUpsert_AllExisting_UpdatesAll)
{
    QVariant id1 = repository->insert(createTestItem("John", 30));
    QVariant id2 = repository->insert(createTestItem("Jane", 25));
    QVariant id3 = repository->insert(createTestItem("Bob", 35));

    QVector<QVariantMap> items;

    QVariantMap update1;
    update1["id"] = id1;
    update1["name"] = "John Updated";
    update1["age"] = 31;
    items.append(update1);

    QVariantMap update2;
    update2["id"] = id2;
    update2["name"] = "Jane Updated";
    update2["age"] = 26;
    items.append(update2);

    QVariantMap update3;
    update3["id"] = id3;
    update3["name"] = "Bob Updated";
    update3["age"] = 36;
    items.append(update3);

    QVector<QVariant> ids = repository->upsert(items);

    EXPECT_EQ(ids.size(), 3);
    EXPECT_EQ(repository->count(), 3);

    QVector<QVariantMap> results = repository->select(Where("id").equals(id1));
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0]["name"].toString(), "John Updated");
    EXPECT_EQ(results[0]["age"].toInt(), 31);
}

TEST_F(DbRepositoryTest, BatchUpsert_MixedNewAndExisting_HandlesCorrectly)
{
    QVariant id1 = repository->insert(createTestItem("John", 30));
    QVariant id2 = repository->insert(createTestItem("Jane", 25));

    QVector<QVariantMap> items;

    QVariantMap existing1;
    existing1["id"] = id1;
    existing1["name"] = "John Updated";
    existing1["age"] = 31;
    items.append(existing1);

    items.append(createTestItem("Bob", 35));

    QVariantMap existing2;
    existing2["id"] = id2;
    existing2["name"] = "Jane Updated";
    existing2["age"] = 26;
    items.append(existing2);

    items.append(createTestItem("Alice", 28));
    items.append(createTestItem("Charlie", 40));

    QVector<QVariant> ids = repository->upsert(items);

    EXPECT_EQ(ids.size(), 5);
    EXPECT_EQ(repository->count(), 5);

    QVector<QVariantMap> updatedJohn = repository->select(Where("id").equals(id1));
    ASSERT_EQ(updatedJohn.size(), 1);
    EXPECT_EQ(updatedJohn[0]["name"].toString(), "John Updated");

    QVector<QVariantMap> newBob = repository->select(Where("name").equals("Bob"));
    EXPECT_EQ(newBob.size(), 1);
}

TEST_F(DbRepositoryTest, BatchUpsert_EmptyVector_ReturnsEmptyVector)
{
    QVector<QVariantMap> items;

    QVector<QVariant> ids = repository->upsert(items);

    EXPECT_TRUE(ids.isEmpty());
}

TEST_F(DbRepositoryTest, BatchUpsert_LargeDataSet_HandlesCorrectly)
{
    QVector<QVariant> existingIds;
    for (int i = 0; i < 50; ++i)
    {
        QVariant id = repository->insert(createTestItem(QString("User%1").arg(i), 20 + i));
        existingIds.append(id);
    }

    QVector<QVariantMap> items;

    for (int i = 0; i < 30; ++i)
    {
        QVariantMap update;
        update["id"] = existingIds[i];
        update["name"] = QString("User%1 Updated").arg(i);
        update["age"] = 30 + i;
        items.append(update);
    }

    for (int i = 50; i < 100; ++i)
    {
        items.append(createTestItem(QString("User%1").arg(i), 20 + i));
    }

    QVector<QVariant> ids = repository->upsert(items);

    EXPECT_EQ(ids.size(), 80);
    EXPECT_EQ(repository->count(), 100);

    QVector<QVariantMap> updated = repository->select(Where("name").like("%Updated%"));
    EXPECT_EQ(updated.size(), 30);
}

TEST_F(DbRepositoryTest, UpdateAll_EmptyVector_ReturnsEmptyVector)
{
    QVector<QVariantMap> items;

    QVector<QVariant> ids = repository->updateAll(items);

    EXPECT_TRUE(ids.isEmpty());
}

TEST_F(DbRepositoryTest, UpdateAll_MultipleItems_ReturnsAllIds)
{
    QVariant id1 = repository->insert(createTestItem("John", 30));
    QVariant id2 = repository->insert(createTestItem("Jane", 25));
    QVariant id3 = repository->insert(createTestItem("Bob", 35));

    QVector<QVariantMap> items;

    QVariantMap update1;
    update1["id"] = id1;
    update1["name"] = "John Updated";
    update1["age"] = 31;
    items.append(update1);

    QVariantMap update2;
    update2["id"] = id2;
    update2["name"] = "Jane Updated";
    update2["age"] = 26;
    items.append(update2);

    QVariantMap update3;
    update3["id"] = id3;
    update3["name"] = "Bob Updated";
    update3["age"] = 36;
    items.append(update3);

    QVector<QVariant> ids = repository->updateAll(items);

    EXPECT_EQ(ids.size(), 3);
    EXPECT_TRUE(ids.contains(id1));
    EXPECT_TRUE(ids.contains(id2));
    EXPECT_TRUE(ids.contains(id3));

    QVector<QVariantMap> results = repository->select();
    for (const auto& result : results)
    {
        EXPECT_TRUE(result["name"].toString().contains("Updated"));
    }
}

TEST_F(DbRepositoryTest, UpdateAll_NonExistentItems_ReturnsEmptyVector)
{
    QVector<QVariantMap> items;

    QVariantMap item1;
    item1["id"] = 999;
    item1["name"] = "NonExistent";
    items.append(item1);

    QVariantMap item2;
    item2["id"] = 998;
    item2["name"] = "AlsoNonExistent";
    items.append(item2);

    QVector<QVariant> ids = repository->updateAll(items);

    EXPECT_TRUE(ids.isEmpty());
}

TEST_F(DbRepositoryTest, UpdateAll_MixedExistentAndNonExistent_ReturnsOnlyExistent)
{
    QVariant id1 = repository->insert(createTestItem("John", 30));

    QVector<QVariantMap> items;

    QVariantMap existing;
    existing["id"] = id1;
    existing["name"] = "John Updated";
    items.append(existing);

    QVariantMap nonExistent;
    nonExistent["id"] = 999;
    nonExistent["name"] = "Ghost";
    items.append(nonExistent);

    QVector<QVariant> ids = repository->updateAll(items);

    EXPECT_EQ(ids.size(), 1);
    EXPECT_EQ(ids[0], id1);
}

TEST_F(DbRepositoryTest, UpsertAll_EmptyVector_ReturnsEmptyVector)
{
    QVector<QVariantMap> items;

    QVector<QVariant> ids = repository->upsertAll(items);

    EXPECT_TRUE(ids.isEmpty());
}

TEST_F(DbRepositoryTest, UpsertAll_AllNew_InsertsAndReturnsIds)
{
    QVector<QVariantMap> items;
    items.append(createTestItem("John", 30));
    items.append(createTestItem("Jane", 25));
    items.append(createTestItem("Bob", 35));

    QVector<QVariant> ids = repository->upsertAll(items);

    EXPECT_EQ(ids.size(), 3);
    EXPECT_EQ(repository->count(), 3);

    for (const auto& id : ids)
    {
        EXPECT_TRUE(id.isValid());
    }
}

TEST_F(DbRepositoryTest, UpsertAll_AllExisting_UpdatesAndReturnsIds)
{
    QVariant id1 = repository->insert(createTestItem("John", 30));
    QVariant id2 = repository->insert(createTestItem("Jane", 25));

    QVector<QVariantMap> items;

    QVariantMap update1;
    update1["id"] = id1;
    update1["name"] = "John Updated";
    update1["age"] = 31;
    items.append(update1);

    QVariantMap update2;
    update2["id"] = id2;
    update2["name"] = "Jane Updated";
    update2["age"] = 26;
    items.append(update2);

    QVector<QVariant> ids = repository->upsertAll(items);

    EXPECT_EQ(ids.size(), 2);
    EXPECT_EQ(repository->count(), 2);

    QVector<QVariantMap> results = repository->select(Where("id").equals(id1));
    EXPECT_EQ(results[0]["name"].toString(), "John Updated");
}

TEST_F(DbRepositoryTest, UpsertAll_MixedExistingAndNew_HandlesCorrectly)
{
    QVariant id1 = repository->insert(createTestItem("John", 30));

    QVector<QVariantMap> items;

    QVariantMap existing;
    existing["id"] = id1;
    existing["name"] = "John Updated";
    existing["age"] = 31;
    items.append(existing);

    items.append(createTestItem("Jane", 25));
    items.append(createTestItem("Bob", 35));

    QVector<QVariant> ids = repository->upsertAll(items);

    EXPECT_EQ(ids.size(), 3);
    EXPECT_EQ(repository->count(), 3);

    QVector<QVariantMap> updatedJohn = repository->select(Where("id").equals(id1));
    EXPECT_EQ(updatedJohn[0]["name"].toString(), "John Updated");

    QVector<QVariantMap> newJane = repository->select(Where("name").equals("Jane"));
    EXPECT_EQ(newJane.size(), 1);
}

TEST_F(DbRepositoryTest, BatchInsert_ItemsWithMissingFields_InsertsWithDefaults)
{
    QVector<QVariantMap> items;

    QVariantMap item1;
    item1["name"] = "John";
    items.append(item1);

    QVariantMap item2;
    item2["name"] = "Jane";
    item2["age"] = 25;
    items.append(item2);

    QVector<QVariant> ids = repository->insert(items);

    EXPECT_EQ(ids.size(), 2);
    EXPECT_EQ(repository->count(), 2);
}

TEST_F(DbRepositoryTest, BatchInsert_SingleItemChunk_ProcessesCorrectly)
{
    QVector<QVariantMap> items;
    items.append(createTestItem("John", 30));
    items.append(createTestItem("Jane", 25));
    items.append(createTestItem("Bob", 35));

    QVector<QVariant> ids = repository->insert(items, 1);

    EXPECT_EQ(ids.size(), 3);
    EXPECT_EQ(repository->count(), 3);
}

TEST_F(DbRepositoryTest, Insert_SingleItem_DelegatesCorrectly)
{
    QVariantMap item = createTestItem("John", 30, "john@example.com");

    QVariant id = repository->insert(item);

    EXPECT_TRUE(id.isValid());
    EXPECT_GT(id.toInt(), 0);
    EXPECT_EQ(repository->count(), 1);
}

TEST_F(DbRepositoryTest, Upsert_SingleItem_WithVector_WorksCorrectly)
{
    QVector<QVariantMap> items;
    items.append(createTestItem("John", 30));

    QVector<QVariant> ids = repository->upsert(items);

    EXPECT_EQ(ids.size(), 1);
    EXPECT_EQ(repository->count(), 1);

    QVariantMap update = createTestItem("John Updated", 31);
    update["id"] = ids[0];
    QVector<QVariantMap> updateItems;
    updateItems.append(update);

    QVector<QVariant> updatedIds = repository->upsert(updateItems);

    EXPECT_EQ(updatedIds.size(), 1);
    EXPECT_EQ(repository->count(), 1);

    QVector<QVariantMap> results = repository->select(Where("id").equals(ids[0]));
    EXPECT_EQ(results[0]["name"].toString(), "John Updated");
}

TEST_F(DbRepositoryTest, BatchUpsert_WithChunking_HandlesLargeDatasets)
{
    QVector<QVariant> existingIds;
    for (int i = 0; i < 150; ++i)
    {
        QVariant id = repository->insert(createTestItem(QString("User%1").arg(i), 20 + i));
        existingIds.append(id);
    }

    QVector<QVariantMap> items;

    for (int i = 0; i < 100; ++i)
    {
        QVariantMap update;
        update["id"] = existingIds[i];
        update["name"] = QString("User%1 Updated").arg(i);
        update["age"] = 30 + i;
        items.append(update);
    }

    for (int i = 150; i < 300; ++i)
    {
        items.append(createTestItem(QString("User%1").arg(i), 20 + i));
    }

    QVector<QVariant> ids = repository->upsert(items, 50);

    EXPECT_EQ(ids.size(), 250);
    EXPECT_EQ(repository->count(), 300);

    QVector<QVariantMap> updated = repository->select(Where("name").like("%Updated%"));
    EXPECT_EQ(updated.size(), 100);
}
