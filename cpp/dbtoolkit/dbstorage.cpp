#include "dbstorage.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

#include "query/select.h"
#include "query/sqlcommand.h"

DbStorage::DbStorage(QSqlDatabase& database)
    : m_database(database)
{
}

QVector<QVariantMap> DbStorage::execute(const Select& query)
{
    QMutexLocker locker(&m_mutex);
    return query.execute(m_database);
}

QVariant DbStorage::execute(const SqlCommand& command)
{
    QMutexLocker locker(&m_mutex);
    return command.execute(m_database);
}

bool DbStorage::beginTransaction()
{
    QMutexLocker locker(&m_mutex);

    if (m_transactionDepth == 0)
    {
        if (!m_database.transaction())
        {
            qWarning() << "Failed to begin transaction:" << m_database.lastError();
            return false;
        }
    }
    else if (!executeStatement("SAVEPOINT " + savepointName(m_transactionDepth),
                               "begin nested transaction"))
    {
        return false;
    }

    ++m_transactionDepth;
    return true;
}

bool DbStorage::commit()
{
    QMutexLocker locker(&m_mutex);

    if (m_transactionDepth == 0)
    {
        qWarning() << "Failed to commit: no active transaction";
        return false;
    }

    if (m_transactionDepth == 1)
    {
        if (!m_database.commit())
        {
            qWarning() << "Failed to commit transaction:" << m_database.lastError();
            return false;
        }
    }
    else if (!executeStatement("RELEASE SAVEPOINT " + savepointName(m_transactionDepth - 1),
                               "commit nested transaction"))
    {
        return false;
    }

    --m_transactionDepth;
    return true;
}

bool DbStorage::rollback()
{
    QMutexLocker locker(&m_mutex);

    if (m_transactionDepth == 0)
    {
        qWarning() << "Failed to rollback: no active transaction";
        return false;
    }

    if (m_transactionDepth == 1)
    {
        if (!m_database.rollback())
        {
            qWarning() << "Failed to rollback transaction:" << m_database.lastError();
            return false;
        }
    }
    else
    {
        const QString savepoint = savepointName(m_transactionDepth - 1);
        if (!executeStatement("ROLLBACK TO SAVEPOINT " + savepoint, "rollback nested transaction")
            || !executeStatement("RELEASE SAVEPOINT " + savepoint, "release nested transaction"))
        {
            return false;
        }
    }

    --m_transactionDepth;
    return true;
}

int DbStorage::transactionDepth() const
{
    QMutexLocker locker(&m_mutex);
    return m_transactionDepth;
}

bool DbStorage::executeStatement(const QString& statement, const QString& failureDescription)
{
    QSqlQuery query(m_database);
    if (!query.exec(statement))
    {
        qWarning() << "Failed to" << failureDescription << ":" << query.lastError();
        return false;
    }
    return true;
}

QString DbStorage::savepointName(int depth) const { return QString("sp%1").arg(depth); }
