#pragma once
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>
#include <optional>

#include "join.h"
#include "order.h"
#include "sqlquery.h"
#include "where.h"

enum class JoinType
{
    Inner,
    Left
};

class Select : public SqlQuery
{
public:
    static QVariantMap extractPrefixedData(const QVariantMap& row, const QString& prefix,
                                           const QStringList& columnKeys);
    static QVariantMap extractColumns(const QVariantMap& row, const QStringList& keys);

    explicit Select(const QStringList& columns = QStringList());

    Select& from(const QString& table);
    Select& from(const Select& subquery);
    Select& as(const QString& alias);
    Select& as(const TableAlias& alias);
    Select& innerJoin(const QString& table, const QString& condition);
    Select& innerJoin(const Join& join);
    Select& leftJoin(const QString& table, const QString& condition);
    Select& leftJoin(const Join& join);
    Select& where(const QString& condition);
    Select& where(const Where& condition);
    Select& orderBy(const QString& orderBy);
    Select& orderBy(const Order& orderClause);
    Select& limit(int limit);
    Select& offset(int offset);
    Select& groupBy(const QString& groupBy);

    QVector<QVariantMap> execute(QSqlDatabase& database) const;
    QString toSql() const override;

    void setColumns(const QStringList& columns);
    bool hasColumns() const;
    bool hasFrom() const;
    QString from() const;

private:
    QStringList m_columns;
    QStringList m_joinColumns;
    QString m_from;
    std::optional<TableAlias> m_fromAlias;
    QStringList m_joins;
    QString m_where;
    QString m_orderBy;
    QString m_groupBy;
    std::optional<int> m_limit;
    std::optional<int> m_offset;

    QStringList selectColumns() const;
    QVariantMap parseRowToNestedMap(const QSqlQuery& query) const;
};
