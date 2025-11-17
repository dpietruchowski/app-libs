#pragma once
#include <QMutex>
#include <QSqlDatabase>
#include <QString>
#include <QVariantMap>
#include <QVector>

class Select;
class SqlCommand;

class DbStorage
{
public:
    explicit DbStorage(QSqlDatabase& database);
    virtual ~DbStorage() = default;

    QVector<QVariantMap> execute(const Select& query);
    QVariant execute(const SqlCommand &command);

    bool beginTransaction();
    bool commit();
    bool rollback();

    QSqlDatabase& database() { return m_database; }

protected:
    QSqlDatabase& m_database;
    mutable QMutex m_mutex;
};
