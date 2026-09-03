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

QVariant DbStorage::execute(const SqlCommand &command)
{
    QMutexLocker locker(&m_mutex);
    return command.execute(m_database);
}

bool DbStorage::beginTransaction()
{
    QMutexLocker locker(&m_mutex);

    if (!m_database.transaction())
    {
        qWarning() << "Failed to begin transaction:" << m_database.lastError();
        return false;
    }

    return true;
}

bool DbStorage::commit()
{
    QMutexLocker locker(&m_mutex);

    if (!m_database.commit())
    {
        qWarning() << "Failed to commit transaction:" << m_database.lastError();
        return false;
    }

    return true;
}

bool DbStorage::rollback()
{
    QMutexLocker locker(&m_mutex);

    if (!m_database.rollback())
    {
        qWarning() << "Failed to rollback transaction:" << m_database.lastError();
        return false;
    }

    return true;
}
