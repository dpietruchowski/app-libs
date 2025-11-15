#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

#include "query/order.h"
#include "query/where.h"

class DbStorage;
class CreateTable;

class DbRepository : public QObject
{
    Q_OBJECT

public:
    explicit DbRepository(const QString& tableName, const QStringList& keys, DbStorage& storage,
                          QObject* parent = nullptr);
    virtual ~DbRepository() = default;

    bool createTable(const CreateTable& tableDefinition);
    void clearTable();

    QVector<QVariantMap> select(const Where& condition = Where(), const Order& order = Order(),
                                int limit = -1, int offset = -1,
                                const QString& groupBy = QString()) const;
    int insert(const QVariantMap& item);
    int update(const QVariantMap& item, const Where& condition = Where());
    int upsert(const QVariantMap& item);
    int remove(const Where& condition);
    bool exists(const Where& condition) const;
    int count(const Where& condition = {}) const;

    int upsertAll(const QVector<QVariantMap>& items);

    DbStorage& storage();
    const DbStorage& storage() const;

private:
    QString m_tableName;
    QStringList m_keys;
    DbStorage& m_storage;

private:
    QVariantMap filterValidKeys(const QVariantMap& item) const;
    Where buildWhereCondition(const QVariantMap& item, const Where& condition) const;
    void logError(const QString& operation) const;
    void logSuccess(const QString& operation, int affectedRows) const;
};
