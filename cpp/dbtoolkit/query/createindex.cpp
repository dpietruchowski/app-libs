#include "createindex.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>

CreateIndex::CreateIndex(const QString& indexName, const QString& tableName)
    : m_indexName(indexName), m_tableName(tableName)
{
}

CreateIndex& CreateIndex::unique()
{
    m_unique = true;
    return *this;
}

CreateIndex& CreateIndex::ifNotExists()
{
    m_ifNotExists = true;
    return *this;
}

CreateIndex& CreateIndex::columns(const QStringList& cols)
{
    m_columns = cols;
    return *this;
}

CreateIndex& CreateIndex::where(const Where& predicate)
{
    m_where = predicate;
    return *this;
}

QString CreateIndex::toSql() const
{
    if (m_indexName.isEmpty() || m_tableName.isEmpty() || m_columns.isEmpty())
    {
        return QString();
    }

    QString sql = "CREATE";

    if (m_unique)
    {
        sql += " UNIQUE";
    }

    sql += " INDEX";

    if (m_ifNotExists)
    {
        sql += " IF NOT EXISTS";
    }

    sql += " " + m_indexName + " ON " + m_tableName + " (";
    sql += m_columns.join(", ");
    sql += ")";

    if (m_where.has_value())
    {
        QString whereSql = m_where->build();
        if (!whereSql.isEmpty())
        {
            sql += " WHERE " + whereSql;
        }
    }

    return sql;
}

QString CreateIndex::build() const
{
    return toSql();
}

QVariant CreateIndex::execute(QSqlDatabase& db) const
{
    QString sql = toSql();

    QSqlQuery query(db);
    if (!query.exec(sql))
    {
        qWarning() << "CreateIndex exec failed:" << query.lastError();
        qWarning() << "SQL:" << sql;
        return 0;
    }

    return 1;
}
