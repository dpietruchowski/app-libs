#include "projection.h"

#include <QDebug>

Projection::Projection(const TableAlias& alias, const QStringList& columns)
    : m_group(alias.prefix())
{
    for (const QString& column : columns)
    {
        m_expressions.append(Expr(alias, column));
    }
}

Projection::Projection(const QString& group, const QList<Expr>& expressions)
    : m_group(group)
    , m_expressions(expressions)
{
}

const QString& Projection::group() const { return m_group; }

QStringList Projection::selectExpressions() const
{
    QStringList expressions;
    for (const Expr& expression : m_expressions)
    {
        expressions.append(
            QString("%1 AS %2").arg(expression.toSql(), outputName(expression.key())));
    }
    return expressions;
}

QVariantMap Projection::extract(const QVariantMap& row) const
{
    QVariantMap values;
    for (const Expr& expression : m_expressions)
    {
        const QString name = outputName(expression.key());
        if (row.contains(name))
        {
            values[expression.key()] = row.value(name);
        }
    }
    return values;
}

QString Projection::outputName(const QString& key) const
{
    return ColumnPrefix(m_group).createColumn(key);
}

ProjectedRow::ProjectedRow() = default;

ProjectedRow::ProjectedRow(const QMap<QString, QVariantMap>& groups)
    : m_groups(groups)
{
}

QVariantMap ProjectedRow::of(const QString& group) const
{
    if (!m_groups.contains(group))
    {
        qWarning() << "Projected row has no group named" << group
                   << "- known groups:" << m_groups.keys();
        return QVariantMap();
    }
    return m_groups.value(group);
}

bool ProjectedRow::contains(const QString& group) const { return m_groups.contains(group); }
