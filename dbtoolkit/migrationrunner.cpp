#include "migrationrunner.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <algorithm>

#include "dbstorage.h"

MigrationRunner::MigrationRunner(DbStorage& storage)
    : m_storage(storage)
{
}

MigrationRunner& MigrationRunner::add(int version, MigrationStep step)
{
    m_migrations.push_back(Migration { version, std::move(step) });
    return *this;
}

int MigrationRunner::currentVersion() const
{
    QSqlQuery query(m_storage.database());
    if (!query.exec("PRAGMA user_version") || !query.next())
    {
        qWarning() << "MigrationRunner: failed to read user_version:" << query.lastError();
        return 0;
    }
    return query.value(0).toInt();
}

bool MigrationRunner::setVersion(int version)
{
    QSqlQuery query(m_storage.database());
    // PRAGMA does not accept bound parameters; version is an internally controlled int.
    if (!query.exec(QStringLiteral("PRAGMA user_version = %1").arg(version)))
    {
        qWarning() << "MigrationRunner: failed to set user_version to" << version << ":"
                   << query.lastError();
        return false;
    }
    return true;
}

bool MigrationRunner::run()
{
    std::sort(m_migrations.begin(), m_migrations.end(),
              [](const Migration& a, const Migration& b) { return a.version < b.version; });

    const int from = currentVersion();

    for (const Migration& migration : m_migrations)
    {
        if (migration.version <= from)
        {
            continue;
        }

        if (!m_storage.beginTransaction())
        {
            return false;
        }

        if (!migration.step(m_storage.database()) || !setVersion(migration.version))
        {
            qWarning() << "MigrationRunner: migration to version" << migration.version
                       << "failed, rolling back";
            m_storage.rollback();
            return false;
        }

        if (!m_storage.commit())
        {
            return false;
        }

        qInfo() << "MigrationRunner: migrated database to version" << migration.version;
    }

    return true;
}
