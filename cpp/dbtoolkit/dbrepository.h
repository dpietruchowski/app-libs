#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QVariantMap>
#include <QVector>

#include "dbstorage.h"
#include "query/insert.h"
#include "query/order.h"
#include "query/where.h"

class CreateTable;

class DbRepository : public QObject
{
    Q_OBJECT

public:
    explicit DbRepository(const QString& tableName, const QString& idKey, const QStringList& keys,
                          DbStorage& storage, QObject* parent = nullptr);
    virtual ~DbRepository() = default;

    bool createTable(const CreateTable& tableDefinition);
    void clearTable();

    QVector<QVariantMap> select(const Where& condition = Where(), const Order& order = Order(),
                                int limit = -1, int offset = -1,
                                const QString& groupBy = QString()) const;

    QVector<QVariant> insert(const QVector<QVariantMap>& items, int chunkSize = 20);
    QVector<QVariant> upsert(const QVector<QVariantMap>& items, int chunkSize = 20);

    QVariant insert(const QVariantMap& item);
    QVariant upsert(const QVariantMap& item);

    int update(const QVariantMap& item, const Where& condition = Where());
    int remove(const Where& condition);

    bool exists(const Where& condition) const;
    int count(const Where& condition = {}) const;
    int count(const Select& select) const;

    QVector<QVariant> updateAll(const QVector<QVariantMap>& items,
                                const Where& condition = Where());
    QVector<QVariant> upsertAll(const QVector<QVariantMap>& items);

    DbStorage& storage();
    const DbStorage& storage() const;

private:
    QString m_tableName;
    QString m_idKey = "id";
    QStringList m_keys;
    DbStorage& m_storage;

private:
    QVector<QVariant> batchInsert(const QVector<QVariantMap>& items);
    QVector<QVariant> batchUpsert(const QVector<QVariantMap>& items);
    QVector<QVariant> executeInsert(Insert& insertCommand, const QVector<QVariantMap>& items,
                                    const QString& operation);

    QVariantMap filterValidKeys(const QVariantMap& item) const;
    QVector<QVariantMap> filterValidItems(const QVector<QVariantMap>& items) const;
    QStringList columnsPresentIn(const QVector<QVariantMap>& items) const;
    Where buildWhereCondition(const QVariantMap& item, const Where& condition) const;
    void logError(const QString& operation) const;
    void logSuccess(const QString& operation, QVariant affectedRows) const;
};
