#include "delete.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>

Delete::Delete()
{
}

Delete::Delete(const QString& table)
    : m_table(table)
{
}

Delete& Delete::from(const QString& table)
{
    m_table = table;
    return *this;
}

Delete& Delete::where(const Where& condition)
{
    m_where = condition.build();
    return *this;
}

Delete& Delete::where(const QString& condition)
{
    m_where = condition;
    return *this;
}

Delete& Delete::all()
{
    m_deleteAll = true;
    return *this;
}

QVariant Delete::execute(QSqlDatabase &database) const
{
    QString sql = toSql();
    
    if (sql.isEmpty())
    {
        qWarning() << "Delete: invalid query";
        return -1;
    }

    if (m_where.isEmpty() && m_table != "sqlite_sequence" && !m_deleteAll)
    {
        qWarning() << "Delete: WHERE clause required for safety (table:" << m_table << ")";
        return -1;
    }

    QSqlQuery query(database);
    if (!query.exec(sql))
    {
        qWarning() << "Delete exec failed:" << query.lastError();
        qWarning() << "SQL:" << sql;
        return -1;
    }
    
    return query.numRowsAffected();
}

QString Delete::toSql() const
{
    if (m_table.isEmpty())
    {
        return QString();
    }
    
    QString sql = QString("DELETE FROM %1").arg(m_table);
    
    if (!m_where.isEmpty())
    {
        sql += " WHERE " + m_where;
    }
    
    return sql;
}

QString Delete::build() const
{
    return toSql();
}

bool Delete::hasTable() const
{
    return !m_table.isEmpty();
}

bool Delete::hasWhere() const
{
    return !m_where.isEmpty();
}
