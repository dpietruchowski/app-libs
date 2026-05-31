#pragma once
#include <QString>
#include <QVector>
#include <functional>

class DbStorage;
class QSqlDatabase;

// Applies ordered schema migrations tracked via SQLite's `PRAGMA user_version`.
//
// Each migration declares the version it brings the database *to*. On run(),
// every migration whose version is greater than the current `user_version` is
// executed in ascending order, each inside its own transaction; `user_version`
// is bumped only after the step succeeds and commits. A failing step rolls back
// and aborts the run, leaving the database at the last successful version.
class MigrationRunner
{
public:
    // Returns true on success. Receives the live database so the step can run
    // arbitrary DDL/DML (ALTER TABLE, data backfills, etc.).
    using MigrationStep = std::function<bool(QSqlDatabase&)>;

    explicit MigrationRunner(DbStorage& storage);

    // `version` is the schema version this step produces; must be > 0 and unique.
    MigrationRunner& add(int version, MigrationStep step);

    // Applies all pending migrations. Returns false if any step fails.
    bool run();

    int currentVersion() const;

private:
    struct Migration
    {
        int version;
        MigrationStep step;
    };

    bool setVersion(int version);

    DbStorage& m_storage;
    QVector<Migration> m_migrations;
};
