#include <QSqlDatabase>
#include <QSqlQuery>
#include <QUuid>
#include <gtest/gtest.h>
#include <memory>

#include "dbtoolkit/dbtoolkit.h"

class DbRepositoryUuidTest : public ::testing::Test
{
protected:
    std::unique_ptr<DbStorage> storage;
    std::unique_ptr<DbRepository> repository;
    QSqlDatabase database;

    void SetUp() override
    {
        database = QSqlDatabase::addDatabase("QSQLITE", "dbrepository_uuid_test_connection");
        database.setDatabaseName(":memory:");
        ASSERT_TRUE(database.open());

        storage = std::make_unique<DbStorage>(database);
        repository = std::make_unique<DbRepository>(
            "uuid_table", "id", QStringList { "id", "name", "value" }, *storage);

        CreateTable table("uuid_table");
        table.ifNotExists()
            .column(Column("id").blob().primaryKey())
            .column(Column("name").text().notNull())
            .column(Column("value").integer());

        repository->createTable(table);
    }

    void TearDown() override
    {
        QSqlQuery query(database);
        query.exec("DROP TABLE IF EXISTS uuid_table");

        repository.reset();
        storage.reset();

        database.close();
        QSqlDatabase::removeDatabase("dbrepository_uuid_test_connection");
    }

    QVariantMap createTestItem(const QString& name, int value, const QUuid& id = QUuid())
    {
        QVariantMap item;
        item["id"] = (id.isNull() ? QUuid::createUuid() : id).toRfc4122();
        item["name"] = name;
        item["value"] = value;
        return item;
    }

    QUuid extractUuid(const QVariant& variant) { return QUuid::fromRfc4122(variant.toByteArray()); }
};

TEST_F(DbRepositoryUuidTest, Insert_WithUuid_ReturnsUuid)
{
    QUuid expectedId = QUuid::createUuid();
    QVariantMap item = createTestItem("Test", 100, expectedId);

    QVariant result = repository->insert(item);

    EXPECT_TRUE(result.isValid());
    QUuid returnedId = extractUuid(result);
    EXPECT_EQ(returnedId, expectedId);
}

TEST_F(DbRepositoryUuidTest, BatchInsert_MultipleItemsWithUuid_ReturnsAllUuids)
{
    QVector<QUuid> expectedIds;
    QVector<QVariantMap> items;

    for (int i = 0; i < 5; ++i)
    {
        QUuid id = QUuid::createUuid();
        expectedIds.append(id);
        items.append(createTestItem(QString("Item%1").arg(i), i * 10, id));
    }

    QVector<QVariant> result = repository->insert(items);

    ASSERT_EQ(result.size(), 5);
    for (int i = 0; i < 5; ++i)
    {
        QUuid returnedId = extractUuid(result[i]);
        EXPECT_EQ(returnedId, expectedIds[i]);
    }

    EXPECT_EQ(repository->count(), 5);
}

TEST_F(DbRepositoryUuidTest, BatchInsert_LargeDatasetWithChunking_InsertsAll)
{
    QVector<QVariantMap> items;
    QVector<QUuid> expectedIds;

    for (int i = 0; i < 250; ++i)
    {
        QUuid id = QUuid::createUuid();
        expectedIds.append(id);
        items.append(createTestItem(QString("User%1").arg(i), i, id));
    }

    QVector<QVariant> result = repository->insert(items, 100);

    EXPECT_EQ(result.size(), 250);
    EXPECT_EQ(repository->count(), 250);

    for (int i = 0; i < 250; ++i)
    {
        QUuid returnedId = extractUuid(result[i]);
        EXPECT_EQ(returnedId, expectedIds[i]);
    }
}

TEST_F(DbRepositoryUuidTest, Update_ExistingItemWithUuid_Updates)
{
    QUuid id = QUuid::createUuid();
    QVariantMap item = createTestItem("Original", 100, id);
    repository->insert(item);

    QVariantMap update = createTestItem("Updated", 200, id);
    int updated = repository->update(update);

    EXPECT_EQ(updated, 1);

    QVector<QVariantMap> results = repository->select();
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0]["name"].toString(), "Updated");
    EXPECT_EQ(results[0]["value"].toInt(), 200);
}

TEST_F(DbRepositoryUuidTest, Upsert_NewItemWithUuid_Inserts)
{
    QUuid id = QUuid::createUuid();
    QVariantMap item = createTestItem("New", 100, id);

    QVariant result = repository->upsert(item);

    EXPECT_TRUE(result.isValid());
    QUuid returnedId = extractUuid(result);
    EXPECT_EQ(returnedId, id);
    EXPECT_EQ(repository->count(), 1);
}

TEST_F(DbRepositoryUuidTest, Upsert_ExistingItemWithUuid_Updates)
{
    QUuid id = QUuid::createUuid();
    QVariantMap item = createTestItem("Original", 100, id);
    repository->insert(item);

    QVariantMap update = createTestItem("Updated", 200, id);
    QVariant result = repository->upsert(update);

    EXPECT_TRUE(result.isValid());
    EXPECT_EQ(repository->count(), 1);

    QVector<QVariantMap> results = repository->select();
    EXPECT_EQ(results[0]["name"].toString(), "Updated");
    EXPECT_EQ(results[0]["value"].toInt(), 200);
}

TEST_F(DbRepositoryUuidTest, BatchUpsert_MixedNewAndExisting_HandlesCorrectly)
{
    QVector<QUuid> existingIds;
    for (int i = 0; i < 3; ++i)
    {
        QUuid id = QUuid::createUuid();
        existingIds.append(id);
        repository->insert(createTestItem(QString("Existing%1").arg(i), i * 10, id));
    }

    QVector<QVariantMap> items;

    items.append(createTestItem("Updated0", 100, existingIds[0]));
    items.append(createTestItem("Updated1", 110, existingIds[1]));

    for (int i = 0; i < 3; ++i)
    {
        items.append(createTestItem(QString("New%1").arg(i), i * 20));
    }

    QVector<QVariant> result = repository->upsert(items);

    EXPECT_EQ(result.size(), 5);
    EXPECT_EQ(repository->count(), 6);

    QVector<QVariantMap> updated = repository->select(Where("name").like("Updated%"));
    EXPECT_EQ(updated.size(), 2);
}

TEST_F(DbRepositoryUuidTest, BatchUpsert_WithChunking_HandlesLargeDatasets)
{
    QVector<QUuid> existingIds;
    for (int i = 0; i < 100; ++i)
    {
        QUuid id = QUuid::createUuid();
        existingIds.append(id);
        repository->insert(createTestItem(QString("User%1").arg(i), i, id));
    }

    QVector<QVariantMap> items;

    for (int i = 0; i < 50; ++i)
    {
        items.append(createTestItem(QString("User%1 Updated").arg(i), i * 2, existingIds[i]));
    }

    for (int i = 100; i < 200; ++i)
    {
        items.append(createTestItem(QString("User%1").arg(i), i));
    }

    QVector<QVariant> result = repository->upsert(items, 50);

    EXPECT_EQ(result.size(), 150);
    EXPECT_EQ(repository->count(), 200);

    QVector<QVariantMap> updated = repository->select(Where("name").like("% Updated"));
    EXPECT_EQ(updated.size(), 50);
}

TEST_F(DbRepositoryUuidTest, UpdateAll_MultipleItemsWithUuid_UpdatesAll)
{
    QVector<QUuid> ids;
    for (int i = 0; i < 3; ++i)
    {
        QUuid id = QUuid::createUuid();
        ids.append(id);
        repository->insert(createTestItem(QString("Item%1").arg(i), i * 10, id));
    }

    QVector<QVariantMap> updates;
    for (int i = 0; i < 3; ++i)
    {
        updates.append(createTestItem(QString("Updated%1").arg(i), i * 20, ids[i]));
    }

    QVector<QVariant> result = repository->updateAll(updates);

    EXPECT_EQ(result.size(), 3);

    for (int i = 0; i < 3; ++i)
    {
        QUuid returnedId = extractUuid(result[i]);
        EXPECT_EQ(returnedId, ids[i]);
    }

    QVector<QVariantMap> allItems = repository->select();
    for (const auto& item : allItems)
    {
        EXPECT_TRUE(item["name"].toString().startsWith("Updated"));
    }
}

TEST_F(DbRepositoryUuidTest, UpsertAll_MixedExistingAndNew_ReturnsAllUuids)
{
    QUuid existingId = QUuid::createUuid();
    repository->insert(createTestItem("Existing", 100, existingId));

    QVector<QVariantMap> items;
    items.append(createTestItem("Updated", 200, existingId));
    items.append(createTestItem("New1", 300));
    items.append(createTestItem("New2", 400));

    QVector<QVariant> result = repository->upsertAll(items);

    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(repository->count(), 3);

    QUuid firstId = extractUuid(result[0]);
    EXPECT_EQ(firstId, existingId);
}

TEST_F(DbRepositoryUuidTest, Insert_EmptyVector_ReturnsEmpty)
{
    QVector<QVariantMap> items;

    QVector<QVariant> result = repository->insert(items);

    EXPECT_TRUE(result.isEmpty());
}

TEST_F(DbRepositoryUuidTest, BatchInsert_PreservesUuidOrder)
{
    QVector<QUuid> expectedIds;
    QVector<QVariantMap> items;

    for (int i = 0; i < 10; ++i)
    {
        QUuid id = QUuid::createUuid();
        expectedIds.append(id);
        items.append(createTestItem(QString("Item%1").arg(i), i, id));
    }

    QVector<QVariant> result = repository->insert(items);

    ASSERT_EQ(result.size(), 10);
    for (int i = 0; i < 10; ++i)
    {
        QUuid returnedId = extractUuid(result[i]);
        EXPECT_EQ(returnedId, expectedIds[i]) << "UUID mismatch at index " << i;
    }
}
