#include "order.h"

Order::Order()
{
}

Order::Order(const QString& column)
    : m_currentColumn(column)
{
}

Order::Order(const QString& alias, const QString& column)
    : Order(TableAlias(alias), column)
{
}

Order::Order(const TableAlias& alias, const QString& column)
    : Order(alias.createColumn(column))
{
}

Order& Order::asc()
{
    finalizeCurrent("ASC");
    return *this;
}

Order& Order::desc()
{
    finalizeCurrent("DESC");
    return *this;
}

Order& Order::then(const QString& column)
{
    m_currentColumn = column;
    return *this;
}

Order& Order::then(const QString& alias, const QString& column)
{
    return then(TableAlias(alias), column);
}

Order& Order::then(const TableAlias& alias, const QString& column)
{
    return then(alias.createColumn(column));
}

QString Order::build() const
{
    return m_orderClauses.join(", ");
}

bool Order::isEmpty() const
{
    return m_orderClauses.isEmpty();
}

void Order::finalizeCurrent(const QString& direction)
{
    if (!m_currentColumn.isEmpty())
    {
        m_orderClauses.append(QString("%1 %2").arg(m_currentColumn, direction));
        m_currentColumn.clear();
    }
}
