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

Update& Update::setRaw(const QString& column, const QString& rawExpression)
{
    m_rawValues.append({ column, rawExpression });
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
    if (m_table.isEmpty() || (m_values.isEmpty() && m_rawValues.isEmpty()))
    {
        return QString();
    }

    QStringList setParts;
    for (const QString& column : m_values.keys())
    {
        setParts.append(column + " = ?");
    }
    for (const auto& rawValue : m_rawValues)
    {
        setParts.append(rawValue.first + " = " + rawValue.second);
    }

    QString sql = QString("UPDATE %1 SET %2").arg(m_table, setParts.join(", "));

    if (!m_where.isEmpty())
    {
        sql += " WHERE " + m_where;
    }

    return sql;
}

QString Update::build() const { return toSql(); }

bool Update::hasTable() const { return !m_table.isEmpty(); }

bool Update::hasWhere() const { return !m_where.isEmpty(); }
