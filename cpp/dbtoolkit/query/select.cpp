#include "select.h"
#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>

QVariantMap Select::extractPrefixedData(const QVariantMap& row, const QString& prefix,
                                        const QStringList& columnKeys)
{
    QVariantMap result;

    for (const QString& key : columnKeys)
    {
        QString prefixedKey = QString("%1_%2").arg(prefix, key);
        if (row.contains(prefixedKey))
        {
            result[key] = row[prefixedKey];
        }
    }

    return result;
}

QVariantMap Select::extractColumns(const QVariantMap& row, const QStringList& keys)
{
    QVariantMap result;
    for (const QString& key : keys)
    {
        if (row.contains(key))
        {
            result[key] = row[key];
        }
    }
    return result;
}

Select::Select(const QStringList& columns)
    : m_columns(columns)
{
}

Select& Select::from(const QString& table)
{
    m_from = table;
    return *this;
}

Select& Select::from(const Select& subquery)
{
    from(QString("(%1)").arg(subquery.toSql()));
    return *this;
}

Select& Select::as(const QString& alias) { return as(TableAlias(alias)); }

Select& Select::as(const TableAlias& alias)
{
    m_fromAlias = alias;
    return *this;
}

Select& Select::innerJoin(const QString& table, const QString& condition)
{
    m_joins.append(QString("INNER JOIN %1 ON %2").arg(table, condition));
    return *this;
}

Select& Select::innerJoin(const Join& join)
{
    m_joinColumns.append(join.columnsWithPrefix());
    return innerJoin(join.tableWithAlias(), join.condition());
}

Select& Select::leftJoin(const QString& table, const QString& condition)
{
    m_joins.append(QString("LEFT JOIN %1 ON %2").arg(table, condition));
    return *this;
}

Select& Select::leftJoin(const Join& join)
{
    m_joinColumns.append(join.columnsWithPrefix());
    return leftJoin(join.tableWithAlias(), join.condition());
}

Select& Select::where(const QString& condition)
{
    m_where = condition;
    return *this;
}

Select& Select::where(const Where& condition)
{
    if (condition.isEmpty())
        return *this;
    return where(condition.build());
}

Select& Select::orderBy(const QString& orderBy)
{
    m_orderBy = orderBy;
    return *this;
}

Select& Select::orderBy(const Order& orderClause)
{
    if (orderClause.isEmpty())
        return *this;
    return orderBy(orderClause.build());
}

Select& Select::limit(int limit)
{
    m_limit = limit;
    return *this;
}

Select& Select::offset(int offset)
{
    m_offset = offset;
    return *this;
}

Select& Select::groupBy(const QString& groupBy)
{
    m_groupBy = groupBy;
    return *this;
}

QString Select::toSql() const
{
    QStringList columns = selectColumns();

    QString sql = QString("SELECT %1").arg(columns.join(", "));

    if (!m_from.isEmpty())
    {
        QString fromClause = m_from;
        if (m_fromAlias.has_value())
        {
            fromClause = QString("%1 %2").arg(m_from, m_fromAlias->prefix());
        }
        sql += QString(" FROM %1").arg(fromClause);
    }

    for (const QString& join : m_joins)
        sql += QString(" %1").arg(join);

    if (!m_where.isEmpty())
        sql += QString(" WHERE %1").arg(m_where);

    if (!m_groupBy.isEmpty())
        sql += QString(" GROUP BY %1").arg(m_groupBy);

    if (!m_orderBy.isEmpty())
        sql += QString(" ORDER BY %1").arg(m_orderBy);

    if (m_limit.has_value())
        sql += QString(" LIMIT %1").arg(m_limit.value());

    if (m_offset.has_value())
        sql += QString(" OFFSET %1").arg(m_offset.value());

    return sql;
}

void Select::setColumns(const QStringList& columns) { m_columns = columns; }

bool Select::hasColumns() const { return !m_columns.isEmpty(); }

bool Select::hasFrom() const { return !m_from.isEmpty(); }

QString Select::from() const { return m_from; }

QVector<QVariantMap> Select::execute(QSqlDatabase& database) const
{
    QVector<QVariantMap> results;
    QString sql = toSql();

    QSqlQuery query(database);

    if (!query.exec(sql))
    {
        qWarning() << "Select error:" << query.lastError();
        qWarning() << "Query:" << sql;
        return results;
    }

    while (query.next())
    {
        results.append(parseRowToNestedMap(query));
    }

    return results;
}

QStringList Select::selectColumns() const
{
    QStringList selectColumns = {};

    if (m_fromAlias.has_value())
    {
        for (const QString& col : m_columns)
        {
            if (col.startsWith("COUNT(") || col.startsWith("SUM(") || col.startsWith("AVG(")
                || col.startsWith("MIN(") || col.startsWith("MAX(") || col.startsWith("DISTINCT("))
            {
                selectColumns.append(col);
            }
            else
            {
                selectColumns.append(m_fromAlias->createColumn(col));
            }
        }
    }
    else
    {
        selectColumns = m_columns;
    }
    selectColumns.append(m_joinColumns);

    return selectColumns;
}

QVariantMap Select::parseRowToNestedMap(const QSqlQuery& query) const
{
    QVariantMap result;
    QSqlRecord record = query.record();
    QSet<QString> seenColumns;

    for (int i = 0; i < record.count(); ++i)
    {
        QString fieldName = record.fieldName(i);
        QVariant value = query.value(i);

        if (result.contains(fieldName))
        {
            qWarning() << "Column name conflict detected:" << fieldName << "at index" << i
                       << "- overwriting previous value";
        }

        result[fieldName] = value;
    }

    return result;
}
