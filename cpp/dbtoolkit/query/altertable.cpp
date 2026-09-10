#include "altertable.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

AlterTable::AlterTable(const QString& tableName)
    : m_tableName(tableName)
{
}

AlterTable& AlterTable::addColumn(const Column& col)
{
    m_columns.append(col.definition());
    return *this;
}

AlterTable& AlterTable::dropColumn(const QString& name)
{
    m_dropColumns.append(name);
    return *this;
}

AlterTable& AlterTable::renameColumn(const QString& from, const QString& to)
{
    m_renames.append({ from, to });
    return *this;
}

QVariant AlterTable::execute(QSqlDatabase& database) const
{
    if (m_tableName.isEmpty())
    {
        return 0;
    }

    const QSqlRecord existing = database.record(m_tableName);

    for (const auto& col : m_columns)
    {
        if (existing.contains(col.name))
        {
            continue;
        }

        const QString sql = "ALTER TABLE " + m_tableName + " ADD COLUMN " + col.toSql();

        QSqlQuery query(database);
        if (!query.exec(sql))
        {
            qWarning() << "AlterTable exec failed:" << query.lastError();
            qWarning() << "SQL:" << sql;
            return 0;
        }
    }

    for (const auto& name : m_dropColumns)
    {
        if (!existing.contains(name))
        {
            continue;
        }

        const QString sql = "ALTER TABLE " + m_tableName + " DROP COLUMN " + name;

        QSqlQuery query(database);
        if (!query.exec(sql))
        {
            qWarning() << "AlterTable exec failed:" << query.lastError();
            qWarning() << "SQL:" << sql;
            return 0;
        }
    }

    for (const auto& rename : m_renames)
    {
        if (!existing.contains(rename.from) || existing.contains(rename.to))
        {
            continue;
        }

        const QString sql = "ALTER TABLE " + m_tableName + " RENAME COLUMN " + rename.from + " TO "
            + rename.to;

        QSqlQuery query(database);
        if (!query.exec(sql))
        {
            qWarning() << "AlterTable exec failed:" << query.lastError();
            qWarning() << "SQL:" << sql;
            return 0;
        }
    }

    return 1;
}

QString AlterTable::toSql() const
{
    if (m_tableName.isEmpty()
        || (m_columns.isEmpty() && m_dropColumns.isEmpty() && m_renames.isEmpty()))
    {
        return QString();
    }

    QStringList statements;
    for (const auto& col : m_columns)
    {
        statements.append("ALTER TABLE " + m_tableName + " ADD COLUMN " + col.toSql());
    }
    for (const auto& name : m_dropColumns)
    {
        statements.append("ALTER TABLE " + m_tableName + " DROP COLUMN " + name);
    }
    for (const auto& rename : m_renames)
    {
        statements.append("ALTER TABLE " + m_tableName + " RENAME COLUMN " + rename.from + " TO "
                          + rename.to);
    }

    return statements.join("; ");
}
