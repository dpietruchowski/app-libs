#include "join.h"

Join::Join(const QString& tableName)
    : m_tableName(tableName)
{
}

Join& Join::as(const QString& alias) { return as(TableAlias(alias)); }

Join& Join::as(const TableAlias& alias)
{
    m_tableAlias = alias;
    return *this;
}

Join& Join::on(const QString& leftAlias, const QString& leftColumn)
{
    return on(TableAlias(leftAlias), leftColumn);
}

Join& Join::on(const TableAlias& leftAlias, const QString& leftColumn)
{
    m_leftAlias = leftAlias;
    m_leftColumn = leftColumn;
    return *this;
}

Join& Join::equals(const QString& rightColumn)
{
    m_rightColumn = rightColumn;
    return *this;
}

Join& Join::withColumns(const QStringList& columnKeys)
{
    m_columnKeys = columnKeys;
    return *this;
}

Join& Join::withPrefix(const QString& prefix) { return withPrefix(ColumnPrefix(prefix)); }

Join& Join::withPrefix(const ColumnPrefix& prefix)
{
    m_prefix = prefix;
    return *this;
}

QString Join::tableName() const
{
    return m_tableName;
}

QString Join::tableAlias() const { return m_tableAlias.prefix(); }

QString Join::tableWithAlias() const
{
    return QString("%1 %2").arg(m_tableName, m_tableAlias.prefix());
}

QString Join::condition() const
{
    return QString("%1 = %2").arg(m_leftAlias.createColumn(m_leftColumn),
                                  m_tableAlias.createColumn(m_rightColumn));
}

QStringList Join::columnsWithPrefix() const
{
    QStringList result;
    for (const QString& key : m_columnKeys)
    {
        if (!m_prefix.has_value())
        {
            result.append(m_tableAlias.createColumn(key));
        }
        else
        {
            result.append(QString("%1 as %2")
                              .arg(m_tableAlias.createColumn(key), m_prefix->createColumn(key)));
        }
    }
    return result;
}

QString Join::prefix() const { return m_prefix->prefix(); }
