#include "expr.h"

Expr::Expr(const TableAlias& alias, const QString& column)
    : m_sql(alias.createColumn(column))
    , m_key(column)
{
}

Expr::Expr(const SqlQuery& expression, const QString& key)
    : m_sql(expression.toSql())
    , m_key(key)
{
}

Expr::Expr(const QString& sql, const QString& key)
    : m_sql(sql)
    , m_key(key)
{
}

const QString& Expr::key() const { return m_key; }

QString Expr::toSql() const { return m_sql; }
