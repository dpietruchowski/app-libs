#pragma once
#include <QString>

#include "sqlquery.h"

class Cast : public SqlQuery
{
public:
    Cast(const QString& expression, const QString& type);

    QString toSql() const override;

private:
    QString m_expression;
    QString m_type;
};
