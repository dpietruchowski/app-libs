#include "pragma.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

Pragma::Pragma(const QString& name)
    : m_name(name)
{
}

Pragma& Pragma::withArgument(const QString& argument)
{
    m_argument = argument;
    return *this;
}

Pragma& Pragma::set(const QVariant& value)
{
    m_value = value;
    return *this;
}

QString Pragma::formatValue(const QVariant& value) const
{
    if (value.typeId() == QMetaType::Bool)
    {
        return value.toBool() ? "ON" : "OFF";
    }
    return value.toString();
}

QString Pragma::toSql() const
{
    QString sql = "PRAGMA " + m_name;

    if (!m_argument.isEmpty())
    {
        sql += "(" + m_argument + ")";
    }

    if (m_value.has_value())
    {
        sql += " = " + formatValue(*m_value);
    }

    return sql;
}

QVariant Pragma::execute(QSqlDatabase& database) const
{
    QString sql = toSql();

    QSqlQuery query(database);
    if (!query.exec(sql))
    {
        qWarning() << "Pragma exec failed:" << query.lastError();
        qWarning() << "SQL:" << sql;
        return 0;
    }

    return 1;
}

QVector<QVariantMap> Pragma::query(QSqlDatabase& database) const
{
    QVector<QVariantMap> result;
    QString sql = toSql();

    QSqlQuery sqlQuery(database);
    if (!sqlQuery.exec(sql))
    {
        qWarning() << "Pragma query failed:" << sqlQuery.lastError();
        qWarning() << "SQL:" << sql;
        return result;
    }

    while (sqlQuery.next())
    {
        QVariantMap row;
        QSqlRecord record = sqlQuery.record();
        for (int i = 0; i < record.count(); ++i)
        {
            row[record.fieldName(i)] = sqlQuery.value(i);
        }
        result.append(row);
    }

    return result;
}
