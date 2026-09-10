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
    QVariant execute(const SqlCommand& command);

    bool beginTransaction();
    bool commit();
    bool rollback();
    int transactionDepth() const;

    QSqlDatabase& database() { return m_database; }

protected:
    QSqlDatabase& m_database;
    mutable QMutex m_mutex;

private:
    bool executeStatement(const QString& statement, const QString& failureDescription);
    QString savepointName(int depth) const;

    int m_transactionDepth = 0;
};
