#include "insert.h"
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

Insert::Insert() { }

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
    m_multipleValues.clear();
    m_multipleValues.append(values);
    return *this;
}

Insert& Insert::batchValues(const QVector<QVariantMap>& multipleValues)
{
    m_multipleValues = multipleValues;
    return *this;
}

Insert& Insert::value(const QString& column, const QVariant& value)
{
    if (m_multipleValues.isEmpty())
    {
        m_multipleValues.append(QVariantMap());
    }
    m_multipleValues[0][column] = value;
    if (!m_columnOrder.contains(column))
    {
        m_columnOrder.append(column);
    }
    return *this;
}

QVariant Insert::execute(QSqlDatabase& database) const
{
    QString sql = toSql();

    if (sql.isEmpty())
    {
        qWarning() << "Insert: invalid query";
        return QVariant();
    }

    if (m_multipleValues.isEmpty())
    {
        qWarning() << "Insert: no values to insert";
        return QVariant();
    }

    QStringList columns = m_columnOrder.isEmpty() ? m_multipleValues.first().keys() : m_columnOrder;

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

    for (const auto& values : m_multipleValues)
    {
        for (const QString& column : columns)
        {
            query.addBindValue(values.value(column));
        }
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
    if (m_table.isEmpty() || m_multipleValues.isEmpty())
    {
        return QString();
    }

    QStringList columns = m_columnOrder.isEmpty() ? m_multipleValues.first().keys() : m_columnOrder;

    if (columns.isEmpty())
    {
        return QString();
    }

    QStringList rowPlaceholders;
    for (int col = 0; col < columns.size(); ++col)
    {
        rowPlaceholders.append("?");
    }
    QString singleRow = QString("(%1)").arg(rowPlaceholders.join(", "));

    QStringList allRows;
    for (int row = 0; row < m_multipleValues.size(); ++row)
    {
        allRows.append(singleRow);
    }

    return QString("INSERT INTO %1 (%2) VALUES %3")
        .arg(m_table)
        .arg(columns.join(", "))
        .arg(allRows.join(", "));
}

bool Insert::hasTable() const { return !m_table.isEmpty(); }
