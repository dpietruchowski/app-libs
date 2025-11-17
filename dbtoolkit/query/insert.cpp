#include "insert.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>

Insert::Insert()
{
}

Insert& Insert::into(const QString& table)
{
    m_table = table;
    return *this;
}

Insert& Insert::columns(const QStringList& columns)
{
    m_columnOrder = columns;
    return *this;
}

Insert& Insert::values(const QVariantMap& values)
{
    m_values = values;
    return *this;
}

Insert& Insert::value(const QString& column, const QVariant& value)
{
    m_values[column] = value;
    if (!m_columnOrder.contains(column))
    {
        m_columnOrder.append(column);
    }
    return *this;
}

QVariant Insert::execute(QSqlDatabase &database) const
{
    QString sql = toSql();
    
    if (sql.isEmpty())
    {
        qWarning() << "Insert: invalid query";
        return QVariant();
    }
    
    QStringList columns = m_columnOrder.isEmpty() ? m_values.keys() : m_columnOrder;
    columns.removeAll("id");
    
    if (columns.isEmpty())
    {
        qWarning() << "Insert: no columns to insert";
        return QVariant();
    }
    
    QSqlQuery query(database);
    if (!query.prepare(sql))
    {
        qWarning() << "Insert prepare failed:" << query.lastError();
        return QVariant();
    }
    
    for (const QString& column : columns)
    {
        query.addBindValue(m_values.value(column));
    }
    
    if (!query.exec())
    {
        qWarning() << "Insert exec failed:" << query.lastError();
        qWarning() << "SQL:" << sql;
        return QVariant();
    }

    return query.lastInsertId();
}

QString Insert::toSql() const
{
    if (m_table.isEmpty() || m_values.isEmpty())
    {
        return QString();
    }
    
    QStringList columns = m_columnOrder.isEmpty() ? m_values.keys() : m_columnOrder;
    columns.removeAll("id");
    
    if (columns.isEmpty())
    {
        return QString();
    }
    
    QStringList valuePlaceholders;
    for (int i = 0; i < columns.size(); ++i)
    {
        valuePlaceholders.append("?");
    }
    
    return QString("INSERT INTO %1 (%2) VALUES (%3)")
        .arg(m_table)
        .arg(columns.join(", "))
        .arg(valuePlaceholders.join(", "));
}

bool Insert::hasTable() const
{
    return !m_table.isEmpty();
}
