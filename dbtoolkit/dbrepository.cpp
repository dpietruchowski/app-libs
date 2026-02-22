#include "dbrepository.h"
#include "dbstorage.h"
#include "query/createtable.h"
#include "query/delete.h"
#include "query/insert.h"
#include "query/select.h"
#include "query/update.h"
#include "query/where.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

DbRepository::DbRepository(const QString& tableName, const QString& idKey, const QStringList& keys,
                           DbStorage& storage, QObject* parent)
    : QObject(parent)
    , m_tableName(tableName)
    , m_idKey(idKey)
    , m_keys(keys)
    , m_storage(storage)
{
}

bool DbRepository::createTable(const CreateTable& tableDefinition)
{
    int result = m_storage.execute(tableDefinition).toInt();
    return result >= 0;
}

void DbRepository::clearTable()
{
    Delete deleteCommand;
    deleteCommand.from(m_tableName).all();

    int rowsAffected = m_storage.execute(deleteCommand).toInt();

    if (rowsAffected >= 0)
    {
        Delete resetSequence;
        resetSequence.from("sqlite_sequence").where(Where("name").equals(m_tableName));
        m_storage.execute(resetSequence);
    }
}

QVector<QVariantMap> DbRepository::select(const Where& condition, const Order& order, int limit,
                                          int offset, const QString& groupBy) const
{
    Select query;
    query.setColumns(m_keys);
    query.from(m_tableName);

    if (!condition.isEmpty())
        query.where(condition);

    if (!order.isEmpty())
        query.orderBy(order);

    if (limit > 0)
        query.limit(limit);

    if (offset > 0)
        query.offset(offset);

    if (!groupBy.isEmpty())
        query.groupBy(groupBy);

    return m_storage.execute(query);
}

QVector<QVariant> DbRepository::batchInsert(const QVector<QVariantMap>& items)
{
    QVector<QVariant> insertedIds;

    if (items.isEmpty())
    {
        return insertedIds;
    }

    Insert insertCommand;
    insertCommand.into(m_tableName);

    QVector<QVariantMap> validItemsList;
    bool hasExplicitId = !items.isEmpty() && items.first().contains(m_idKey)
        && items.first().value(m_idKey).isValid();

    for (const auto& item : items)
    {
        QVariantMap validItem = filterValidKeys(item);
        if (!hasExplicitId)
        {
            validItem.remove(m_idKey);
        }
        validItemsList.append(validItem);
    }

    insertCommand.batchValues(validItemsList);

    QVariant result = m_storage.execute(insertCommand);

    if (!result.isValid())
    {
        logError("inserting");
        return insertedIds;
    }

    logSuccess("Inserted", items.size());

    for (int i = 0; i < validItemsList.size(); ++i)
    {
        const auto& validItem = validItemsList[i];
        if (validItem.contains(m_idKey))
        {
            insertedIds.append(validItem[m_idKey]);
        }
        else if (result.isValid())
        {
            insertedIds.append(QVariant(result.toLongLong() + i));
        }
    }

    return insertedIds;
}

QVector<QVariant> DbRepository::batchUpsert(const QVector<QVariantMap>& items)
{
    QVector<QVariant> allIds;

    if (items.isEmpty())
    {
        return allIds;
    }

    auto existingIds = batchExists(items);
    QMap<int, QVariantMap> itemsToInsert;
    QMap<int, QVariantMap> itemsToUpdate;

    for (int i = 0; i < items.size(); ++i)
    {
        const auto& item = items[i];
        if (item.contains(m_idKey) && existingIds.contains(item.value(m_idKey)))
        {
            itemsToUpdate.insert(i, item);
        }
        else
        {
            itemsToInsert.insert(i, item);
        }
    }

    allIds.resize(items.size());

    if (!itemsToInsert.isEmpty())
    {
        QVector<QVariant> insertedIds = batchInsert(itemsToInsert.values().toVector());
        int idx = 0;
        for (int originalIndex : itemsToInsert.keys())
        {
            allIds[originalIndex] = insertedIds[idx++];
        }
    }

    if (!itemsToUpdate.isEmpty())
    {
        QVector<QVariant> updatedIds = updateAll(itemsToUpdate.values().toVector());
        int idx = 0;
        for (int originalIndex : itemsToUpdate.keys())
        {
            allIds[originalIndex] = updatedIds[idx++];
        }
    }

    logSuccess("Batch upserted", allIds.size());
    return allIds;
}

QVector<QVariant> DbRepository::insert(const QVector<QVariantMap>& items, int chunkSize)
{
    QVector<QVariant> allInsertedIds;

    if (items.isEmpty())
    {
        return allInsertedIds;
    }

    for (int i = 0; i < items.size(); i += chunkSize)
    {
        int remaining = items.size() - i;
        int batchSize = qMin(chunkSize, remaining);

        QVector<QVariantMap> chunk;
        chunk.reserve(batchSize);
        for (int j = 0; j < batchSize; ++j)
        {
            chunk.append(items[i + j]);
        }

        QVector<QVariant> chunkIds = batchInsert(chunk);
        allInsertedIds.append(chunkIds);
    }

    return allInsertedIds;
}

QVector<QVariant> DbRepository::upsert(const QVector<QVariantMap>& items, int chunkSize)
{
    QVector<QVariant> allUpsertedIds;

    if (items.isEmpty())
    {
        return allUpsertedIds;
    }

    for (int i = 0; i < items.size(); i += chunkSize)
    {
        int remaining = items.size() - i;
        int batchSize = qMin(chunkSize, remaining);

        QVector<QVariantMap> chunk;
        chunk.reserve(batchSize);
        for (int j = 0; j < batchSize; ++j)
        {
            chunk.append(items[i + j]);
        }

        QVector<QVariant> chunkIds = batchUpsert(chunk);
        allUpsertedIds.append(chunkIds);
    }

    return allUpsertedIds;
}

QVariant DbRepository::insert(const QVariantMap& item)
{
    auto result = batchInsert(QVector<QVariantMap> { item });

    if (result.isEmpty())
    {
        return QVariant();
    }

    return result.first();
}

int DbRepository::update(const QVariantMap& item, const Where& condition)
{
    Update updateCommand;
    updateCommand.table(m_tableName);

    QVariantMap validItems = filterValidKeys(item);
    validItems.remove(m_idKey);

    for (auto it = validItems.constBegin(); it != validItems.constEnd(); ++it)
    {
        updateCommand.set(it.key(), it.value());
    }

    Where whereCondition = buildWhereCondition(item, condition);
    if (whereCondition.isEmpty())
    {
        logError("updating: no valid condition");
        return 0;
    }

    updateCommand.where(whereCondition);

    int rowsAffected = m_storage.execute(updateCommand).toInt();

    if (rowsAffected < 0)
    {
        logError("updating");
        return 0;
    }

    logSuccess("Updated", rowsAffected);

    return rowsAffected;
}

QVariant DbRepository::upsert(const QVariantMap& item)
{
    if (exists(Where(m_idKey).equals(item.value(m_idKey))))
    {
        return update(item);
    }

    QVariant result = insert(item);

    if (!result.isValid())
    {
        qWarning() << "[" << m_tableName << "] Upsert failed for item:" << item;
        return QVariant();
    }

    return result;
}

int DbRepository::remove(const Where& condition)
{
    Delete deleteCommand;
    deleteCommand.from(m_tableName);

    if (!condition.isEmpty())
    {
        deleteCommand.where(condition);
    }

    int rowsAffected = m_storage.execute(deleteCommand).toInt();

    if (rowsAffected < 0)
    {
        logError("deleting");
        return -1;
    }

    logSuccess("Deleted", rowsAffected);
    return rowsAffected;
}

bool DbRepository::exists(const Where& condition) const
{
    if (condition.isEmpty())
    {
        qWarning() << "[" << m_tableName << "] Cannot check existence: condition is empty";
        return false;
    }

    Select query({ "1" });
    query.from(m_tableName).where(condition).limit(1);

    return !m_storage.execute(query).isEmpty();
}

QList<QVariant> DbRepository::batchExists(const QVector<QVariantMap>& items) const
{
    QList<QVariant> existingIds;

    if (items.isEmpty())
    {
        return existingIds;
    }

    QVariantList idsToCheck;
    for (const auto& item : items)
    {
        if (item.contains(m_idKey) && item.value(m_idKey).isValid())
        {
            idsToCheck.append(item.value(m_idKey));
        }
    }

    if (idsToCheck.isEmpty())
    {
        return existingIds;
    }

    Where where(m_idKey);
    where.in(idsToCheck);

    auto results = select(where);

    for (const auto& row : results)
    {
        existingIds.append(row[m_idKey]);
    }

    return existingIds;
}

int DbRepository::count(const Where& condition) const
{
    Select query({ "COUNT(*)" });
    query.from(m_tableName);

    if (!condition.isEmpty())
    {
        query.where(condition);
    }

    return count(query);
}

int DbRepository::count(const Select& select) const
{
    auto results = m_storage.execute(select);

    if (results.isEmpty())
    {
        qWarning() << "[" << m_tableName << "] Error executing count";
        return 0;
    }

    return results.first().value("COUNT(*)").toInt();
}

QVector<QVariant> DbRepository::updateAll(const QVector<QVariantMap>& items, const Where& condition)
{
    QVector<QVariant> updatedIds;
    for (const auto& item : items)
    {
        int updated = update(item, condition);
        if (updated > 0 && item.contains(m_idKey))
        {
            updatedIds.append(item.value(m_idKey));
        }
    }
    return updatedIds;
}

QVector<QVariant> DbRepository::upsertAll(const QVector<QVariantMap>& items)
{
    return upsert(items, items.size());
}

DbStorage& DbRepository::storage() { return m_storage; }

const DbStorage& DbRepository::storage() const { return m_storage; }

QVariantMap DbRepository::filterValidKeys(const QVariantMap& item) const
{
    QVariantMap filtered;
    for (const QString& key : m_keys)
    {
        if (item.contains(key))
        {
            filtered[key] = item.value(key);
        }
    }
    return filtered;
}

Where DbRepository::buildWhereCondition(const QVariantMap& item, const Where& condition) const
{
    if (!condition.isEmpty())
    {
        return condition;
    }

    if (item.contains(m_idKey) && item.value(m_idKey).isValid())
    {
        return Where(m_idKey).equals(item.value(m_idKey));
    }

    return Where();
}

void DbRepository::logError(const QString& operation) const
{
    qWarning() << "[" << m_tableName << "] Error" << operation;
}

void DbRepository::logSuccess(const QString& operation, QVariant affectedRows) const
{
    qDebug() << "[" << m_tableName << "]" << operation << affectedRows << "row(s)";
}
