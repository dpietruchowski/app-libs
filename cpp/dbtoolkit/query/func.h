#pragma once
#include <QString>
#include <QStringList>

#include "sqlquery.h"

class Func : public SqlQuery
{
public:
    Func(const QString& name, const QStringList& arguments);

    static Func coalesce(const QString& expression, const QString& fallback);
    static Func max(const QStringList& arguments);
    static Func min(const QStringList& arguments);
    static Func count(const QString& expression);
    static Func countDistinct(const QString& expression);

    QString toSql() const override;

private:
    QString m_name;
    QStringList m_arguments;
};
