#pragma once
#include <QList>
#include <QPair>
#include <QString>
#include <QVariant>

#include "sqlquery.h"

class Case : public SqlQuery
{
public:
    explicit Case(const QString& subject);

    Case& when(const QVariant& match, const QVariant& result);
    Case& otherwise(const QString& rawExpression);

    QString toSql() const override;

private:
    QString m_subject;
    QList<QPair<QVariant, QVariant>> m_branches;
    QString m_else;

    QString formatValue(const QVariant& value) const;
};
