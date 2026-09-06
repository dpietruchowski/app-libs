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
    if (items.isEmpty())
    {
        return {};
    }

    const QVector<QVariantMap> validItems = filterValidItems(items);

    Insert insertCommand;
    insertCommand.into(m_tableName).columns(columnsPresentIn(validItems));

    return executeInsert(insertCommand, validItems, "Inserted");
}

QVector<QVariant> DbRepository::batchUpsert(const QVector<QVariantMap>& items)
{
    if (items.isEmpty())
    {
        return {};
    }

    const QVector<QVariantMap> validItems = filterValidItems(items);
    const QStringList columns = columnsPresentIn(validItems);

    Insert insertCommand;
    insertCommand.into(m_tableName).columns(columns);
    if (columns.contains(m_idKey))
    {
        insertCommand.onConflict({ m_idKey });
    }

    return executeInsert(insertCommand, validItems, "Upserted");
}

QVector<QVariant> DbRepository::executeInsert(Insert& insertCommand,
                                              const QVector<QVariantMap>& items,
                                              const QString& operation)
{
    insertCommand.batchValues(items);

    QVariant result = m_storage.execute(insertCommand);

    if (!result.isValid())
    {
        qWarning() << "[" << m_tableName << "] Insert failed:";
        qWarning() << "Items to insert:";
        for (int i = 0; i < items.size(); ++i)
        {
            qWarning() << "  Item" << i << ":" << items[i];
        }
        logError("inserting");
        return {};
    }

    logSuccess(operation, items.size());

    QVector<QVariant> ids;
    for (int i = 0; i < items.size(); ++i)
    {
        const QVariantMap& item = items[i];
        if (item.contains(m_idKey))
        {
            ids.append(item[m_idKey]);
        }
        else
        {
            ids.append(QVariant(result.toLongLong() + i));
        }
    }

    return ids;
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
    auto result = batchUpsert(QVector<QVariantMap> { item });

    if (result.isEmpty())
    {
        qWarning() << "[" << m_tableName << "] Upsert failed for item:" << item;
        return QVariant();
    }

    return result.first();
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

QVector<QVariantMap> DbRepository::filterValidItems(const QVector<QVariantMap>& items) const
{
    bool hasExplicitId = false;
    for (const QVariantMap& item : items)
    {
        if (item.value(m_idKey).isValid())
        {
            hasExplicitId = true;
            break;
        }
    }

    QVector<QVariantMap> validItems;
    validItems.reserve(items.size());
    for (const QVariantMap& item : items)
    {
        QVariantMap validItem = filterValidKeys(item);
        if (!hasExplicitId || !validItem.value(m_idKey).isValid())
        {
            validItem.remove(m_idKey);
        }
        validItems.append(validItem);
    }
    return validItems;
}

QStringList DbRepository::columnsPresentIn(const QVector<QVariantMap>& items) const
{
    QStringList columns;
    for (const QString& key : m_keys)
    {
        for (const QVariantMap& item : items)
        {
            if (item.contains(key))
            {
                columns.append(key);
                break;
            }
        }
    }
    return columns;
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
