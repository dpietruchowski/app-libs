#pragma once
#include <QList>
#include <QString>
#include <QVariant>

#include "sqlquery.h"
#include "where.h"

class Case : public SqlQuery
{
public:
    Case();
    explicit Case(const QString& subject);

    Case& when(const QVariant& match, const QVariant& result);
    Case& when(const Where& condition, const SqlQuery& result);
    Case& otherwise(const QString& rawExpression);
    Case& otherwise(const SqlQuery& result);

    QString toSql() const override;

private:
    struct Branch
    {
        QString condition;
        QString result;
    };

    QString m_subject;
    QList<Branch> m_branches;
    QString m_else;

    QString formatValue(const QVariant& value) const;
};
