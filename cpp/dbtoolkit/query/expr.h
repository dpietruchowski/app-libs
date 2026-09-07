#pragma once
#include <QString>

#include "alias.h"
#include "sqlquery.h"

class Expr : public SqlQuery
{
public:
    Expr(const TableAlias& alias, const QString& column);
    Expr(const SqlQuery& expression, const QString& key);
    Expr(const QString& sql, const QString& key);

    const QString& key() const;

    QString toSql() const override;

private:
    QString m_sql;
    QString m_key;
};
