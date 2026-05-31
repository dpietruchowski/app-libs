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

    return 1;
}

QString AlterTable::toSql() const
{
    if (m_tableName.isEmpty() || m_columns.isEmpty())
    {
        return QString();
    }

    QStringList statements;
    for (const auto& col : m_columns)
    {
        statements.append("ALTER TABLE " + m_tableName + " ADD COLUMN " + col.toSql());
    }

    return statements.join("; ");
}
