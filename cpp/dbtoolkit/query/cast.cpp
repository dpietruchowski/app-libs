#include "cast.h"

Cast::Cast(const QString& expression, const QString& type)
    : m_expression(expression)
    , m_type(type)
{
}

QString Cast::toSql() const
{
    return QString("CAST(%1 AS %2)").arg(m_expression, m_type);
}
