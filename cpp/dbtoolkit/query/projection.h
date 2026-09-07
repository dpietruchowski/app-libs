#pragma once
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "alias.h"
#include "expr.h"

class Projection
{
public:
    Projection(const TableAlias& alias, const QStringList& columns);
    Projection(const QString& group, const QList<Expr>& expressions);

    const QString& group() const;
    QStringList selectExpressions() const;
    QVariantMap extract(const QVariantMap& row) const;

private:
    QString outputName(const QString& key) const;

    QString m_group;
    QList<Expr> m_expressions;
};

class ProjectedRow
{
public:
    ProjectedRow();
    explicit ProjectedRow(const QMap<QString, QVariantMap>& groups);

    QVariantMap of(const QString& group) const;
    bool contains(const QString& group) const;

private:
    QMap<QString, QVariantMap> m_groups;
};
