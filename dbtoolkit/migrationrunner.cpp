#include "migrationrunner.h"

#include <QDebug>
#include <QVariantMap>
#include <algorithm>

#include "dbstorage.h"
#include "query/pragma.h"

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
    QVector<QVariantMap> rows = Pragma("user_version").query(m_storage.database());
    if (rows.isEmpty())
    {
        qWarning() << "MigrationRunner: failed to read user_version";
        return 0;
    }
    return rows.first().value("user_version").toInt();
}

bool MigrationRunner::setVersion(int version)
{
    if (!Pragma("user_version").set(version).execute(m_storage.database()).toBool())
    {
        qWarning() << "MigrationRunner: failed to set user_version to" << version;
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
