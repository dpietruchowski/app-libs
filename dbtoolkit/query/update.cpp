#include "update.h"
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

Update::Update() { }

Update::Update(const QString& table)
    : m_table(table)
{
}

Update& Update::table(const QString& table)
{
    m_table = table;
    return *this;
}

Update& Update::set(const QVariantMap& values)
{
    m_values = values;
    return *this;
}

Update& Update::set(const QString& column, const QVariant& value)
{
    m_values[column] = value;
    return *this;
}

Update& Update::where(const Where& condition)
{
    m_where = condition.build();
    return *this;
}

Update& Update::where(const QString& condition)
{
    m_where = condition;
    return *this;
}

QVariant Update::execute(QSqlDatabase& database) const
{
    QStringList columns = m_values.keys();
    QString sql = toSql();

    if (sql.isEmpty())
    {
        qWarning() << "Update: invalid query";
        return -1;
    }

    QSqlQuery query(database);
    if (!query.prepare(sql))
    {
        qWarning() << "Update prepare failed:" << query.lastError();
        return -1;
    }

    for (const QString& column : columns)
    {
        query.addBindValue(m_values.value(column));
    }

    if (!query.exec())
    {
        qWarning() << "Update exec failed:" << query.lastError();
        qWarning() << "SQL:" << sql;
        return -1;
    }

    return query.numRowsAffected();
}

QString Update::toSql() const
{
    if (m_table.isEmpty() || m_values.isEmpty())
    {
        return QString();
    }

    QStringList columns = m_values.keys();

    if (columns.isEmpty())
    {
        return QString();
    }

    QString whereClause = m_where;

    QStringList setParts;
    for (const QString& column : columns)
    {
        setParts.append(column + " = ?");
    }

    QString sql = QString("UPDATE %1 SET %2").arg(m_table).arg(setParts.join(", "));

    if (!whereClause.isEmpty())
    {
        sql += " WHERE " + whereClause;
    }

    return sql;
}

QString Update::build() const { return toSql(); }

bool Update::hasTable() const { return !m_table.isEmpty(); }

bool Update::hasWhere() const { return !m_where.isEmpty(); }
