#pragma once
#include <QString>
#include <QVariant>

#include "sqlquery.h"

class SqlQuery
{
public:
    virtual ~SqlQuery() = default;

    virtual QString toSql() const = 0;
    virtual QString build() const { return toSql(); }
};
