#include "dbrepository.h"
#include "dbstorage.h"
#include "query/createtable.h"
#include "query/delete.h"
#include "query/insert.h"
#include "query/select.h"
#include "query/update.h"
#include "query/where.h"

#include <QDebug>

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

QVariant DbRepository::insert(const QVariantMap& item)
{
    Insert insertCommand;
    insertCommand.into(m_tableName);

    QVariantMap validItems = filterValidKeys(item);
    for (auto it = validItems.constBegin(); it != validItems.constEnd(); ++it)
    {
        insertCommand.value(it.key(), it.value());
    }

    QVariant insertedId = m_storage.execute(insertCommand);

    if (!insertedId.isValid())
    {
        logError("inserting");
        return QVariant();
    }

    logSuccess("Inserted ID:", insertedId);

    if (!m_keys.isEmpty() && validItems.contains(m_keys.first()))
    {
        return validItems[m_keys.first()];
    }

    return QVariant(insertedId);
}

int DbRepository::update(const QVariantMap& item, const Where& condition)
{
    Update updateCommand;
    updateCommand.table(m_tableName);

    QVariantMap validItems = filterValidKeys(item);
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

    if (!m_keys.isEmpty() && validItems.contains(m_keys.first()))
    {
        return validItems[m_keys.first()].toInt();
    }

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

int DbRepository::count(const Where& condition) const
{
    Select query({ "COUNT(*)" });
    query.from(m_tableName);

    if (!condition.isEmpty())
    {
        query.where(condition);
    }

    auto results = m_storage.execute(query);

    if (results.isEmpty())
    {
        qWarning() << "[" << m_tableName << "] Error executing count";
        return 0;
    }

    return results.first().value("COUNT(*)").toInt();
}

int DbRepository::upsertAll(const QVector<QVariantMap>& items)
{
    int count = 0;
    for (const auto& item : items)
    {
        QVariant result = upsert(item);
        if (result.isValid())
        {
            count++;
        }
    }
    return count;
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
