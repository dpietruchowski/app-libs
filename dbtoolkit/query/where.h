#pragma once
#include <QString>
#include <QVariant>
#include <QStringList>
#include <QList>

#include "alias.h"

class Where
{
public:
    Where();
    explicit Where(const QString& column);
    Where(const QString& alias, const QString& column);
    Where(const TableAlias& alias, const QString& column);

    Where& equals(const QVariant& value);
    Where& notEquals(const QVariant& value);
    Where& lessThan(const QVariant& value);
    Where& greaterThan(const QVariant& value);
    Where& lessThanOrEquals(const QVariant& value);
    Where& lessOrEqual(const QVariant& value);
    Where& greaterThanOrEquals(const QVariant& value);
    Where& greaterOrEqual(const QVariant& value);
    Where& like(const QString& pattern);
    Where& in(const QVariantList& values);
    Where& in(const QStringList& values);
    Where& in(const QList<int>& values);
    Where& between(const QVariant& min, const QVariant& max);
    Where& isNull();
    Where& isNotNull();

    Where& not_(const QString& column);
    Where& not_(const QString& alias, const QString& column);
    Where& not_(const TableAlias& alias, const QString& column);
    Where& not_(const Where& condition);
    Where& and_(const QString& column);
    Where& and_(const QString& alias, const QString& column);
    Where& and_(const TableAlias& alias, const QString& column);
    Where& and_(const Where& condition);
    Where& or_(const QString& column);
    Where& or_(const QString& alias, const QString& column);
    Where& or_(const TableAlias& alias, const QString& column);
    Where& or_(const Where& condition);
    Where& raw(const Where& condition);

    QString build() const;
    bool isEmpty() const;

private:
    QString m_condition;
    QString m_currentColumn;

    void appendOperator(const QString& op, const QVariant& value);
    QString formatValue(const QVariant& value) const;
    Where& raw(const QString& rawSql);
};
