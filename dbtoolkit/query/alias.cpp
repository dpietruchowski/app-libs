#include "alias.h"

Alias::Alias(const QString& prefix, const QString& separator)
    : m_prefix(prefix)
    , m_separator(separator)
{
}

QString Alias::createColumn(const QString& column) const
{
    if (m_prefix.isEmpty())
        return column;
    
    return QString("%1%2%3").arg(m_prefix, m_separator, column);
}

TableAlias::TableAlias(const QString& prefix)
    : Alias(prefix, ".")
{
}

ColumnPrefix::ColumnPrefix(const QString& prefix)
    : Alias(prefix, "_")
{
}
