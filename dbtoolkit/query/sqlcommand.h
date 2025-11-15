#pragma once
#include <QSqlDatabase>

#include "sqlquery.h"

class SqlCommand : public SqlQuery
{
public:
    virtual ~SqlCommand() = default;

    virtual int execute(QSqlDatabase& database) const = 0;
};
