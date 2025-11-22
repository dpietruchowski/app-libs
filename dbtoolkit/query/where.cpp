#include "where.h"

Where::Where() { }

Where::Where(const QString& column)
    : m_currentColumn(column)
{
}

Where::Where(const QString& alias, const QString& column)
    : Where(TableAlias(alias), column)
{
}

Where::Where(const TableAlias& alias, const QString& column)
    : Where(alias.createColumn(column))
{
}

Where& Where::equals(const QVariant& value)
{
    appendOperator("=", value);
    return *this;
}

Where& Where::notEquals(const QVariant& value)
{
    appendOperator("!=", value);
    return *this;
}

Where& Where::lessThan(const QVariant& value)
{
    appendOperator("<", value);
    return *this;
}

Where& Where::greaterThan(const QVariant& value)
{
    appendOperator(">", value);
    return *this;
}

Where& Where::lessThanOrEquals(const QVariant& value)
{
    appendOperator("<=", value);
    return *this;
}

Where& Where::lessOrEqual(const QVariant& value) { return lessThanOrEquals(value); }

Where& Where::greaterThanOrEquals(const QVariant& value)
{
    appendOperator(">=", value);
    return *this;
}

Where& Where::greaterOrEqual(const QVariant& value) { return greaterThanOrEquals(value); }

Where& Where::like(const QString& pattern)
{
    if (!m_currentColumn.isEmpty())
    {
        if (!m_condition.isEmpty())
            m_condition += " ";
        m_condition += QString("%1 LIKE '%2'").arg(m_currentColumn, pattern);
        m_currentColumn.clear();
    }
    return *this;
}

Where& Where::in(const QVariantList& values)
{
    if (!m_currentColumn.isEmpty() && !values.isEmpty())
    {
        if (!m_condition.isEmpty())
            m_condition += " ";

        QStringList formattedValues;
        for (const QVariant& value : values)
        {
            formattedValues << formatValue(value);
        }

        m_condition += QString("%1 IN (%2)").arg(m_currentColumn, formattedValues.join(", "));
        m_currentColumn.clear();
    }
    return *this;
}

Where& Where::in(const QStringList& values)
{
    if (!m_currentColumn.isEmpty() && !values.isEmpty())
    {
        if (!m_condition.isEmpty())
            m_condition += " ";

        QStringList formattedValues;
        for (const QString& value : values)
        {
            formattedValues << formatValue(value);
        }

        m_condition += QString("%1 IN (%2)").arg(m_currentColumn, formattedValues.join(", "));
        m_currentColumn.clear();
    }
    return *this;
}

Where& Where::in(const QList<int>& values)
{
    if (!m_currentColumn.isEmpty() && !values.isEmpty())
    {
        if (!m_condition.isEmpty())
            m_condition += " ";

        QStringList formattedValues;
        for (int value : values)
        {
            formattedValues << QString::number(value);
        }

        m_condition += QString("%1 IN (%2)").arg(m_currentColumn, formattedValues.join(", "));
        m_currentColumn.clear();
    }
    return *this;
}

Where& Where::between(const QVariant& min, const QVariant& max)
{
    if (!m_currentColumn.isEmpty())
    {
        if (!m_condition.isEmpty())
            m_condition += " ";
        m_condition += QString("%1 BETWEEN %2 AND %3")
                           .arg(m_currentColumn, formatValue(min), formatValue(max));
        m_currentColumn.clear();
    }
    return *this;
}

Where& Where::isNull()
{
    if (!m_currentColumn.isEmpty())
    {
        if (!m_condition.isEmpty())
            m_condition += " ";
        m_condition += QString("%1 IS NULL").arg(m_currentColumn);
        m_currentColumn.clear();
    }
    return *this;
}

Where& Where::isNotNull()
{
    if (!m_currentColumn.isEmpty())
    {
        if (!m_condition.isEmpty())
            m_condition += " ";
        m_condition += QString("%1 IS NOT NULL").arg(m_currentColumn);
        m_currentColumn.clear();
    }
    return *this;
}

Where& Where::not_(const QString& column)
{
    if (!m_condition.isEmpty())
        m_condition += " NOT";
    m_currentColumn = column;
    return *this;
}

Where& Where::not_(const QString& alias, const QString& column)
{
    return not_(TableAlias(alias), column);
}

Where& Where::not_(const TableAlias& alias, const QString& column)
{
    return not_(alias.createColumn(column));
}

Where& Where::not_(const Where& condition)
{
    if (!condition.isEmpty())
    {
        if (!m_condition.isEmpty())
            m_condition += " NOT ";
        m_condition += QString("(%1)").arg(condition.build());
    }
    return *this;
}

Where& Where::and_(const QString& column)
{
    if (!m_condition.isEmpty())
        m_condition += " AND";
    m_currentColumn = column;
    return *this;
}

Where& Where::and_(const QString& alias, const QString& column)
{
    return and_(TableAlias(alias), column);
}

Where& Where::and_(const TableAlias& alias, const QString& column)
{
    return and_(alias.createColumn(column));
}

Where& Where::and_(const Where& condition)
{
    if (!condition.isEmpty())
    {
        if (!m_condition.isEmpty())
            m_condition += " AND ";
        m_condition += QString("(%1)").arg(condition.build());
    }
    return *this;
}

Where& Where::or_(const QString& column)
{
    if (!m_condition.isEmpty())
        m_condition += " OR";
    m_currentColumn = column;
    return *this;
}

Where& Where::or_(const QString& alias, const QString& column)
{
    return or_(TableAlias(alias), column);
}

Where& Where::or_(const TableAlias& alias, const QString& column)
{
    return or_(alias.createColumn(column));
}

Where& Where::or_(const Where& condition)
{
    if (!condition.isEmpty())
    {
        if (!m_condition.isEmpty())
            m_condition += " OR ";
        m_condition += QString("(%1)").arg(condition.build());
    }
    return *this;
}

Where& Where::raw(const Where& condition)
{
    if (!condition.isEmpty())
    {
        if (!m_condition.isEmpty())
            m_condition += " ";
        m_condition += QString("(%1)").arg(condition.build());
    }
    return *this;
}

Where& Where::raw(const QString& rawSql)
{
    if (!rawSql.isEmpty())
    {
        if (!m_condition.isEmpty())
            m_condition += " ";
        m_condition += rawSql;
    }
    return *this;
}

QString Where::build() const { return m_condition; }

bool Where::isEmpty() const { return m_condition.isEmpty(); }

void Where::appendOperator(const QString& op, const QVariant& value)
{
    if (!m_currentColumn.isEmpty())
    {
        if (!m_condition.isEmpty())
            m_condition += " ";
        m_condition += QString("%1 %2 %3").arg(m_currentColumn, op, formatValue(value));
        m_currentColumn.clear();
    }
}

QString Where::formatValue(const QVariant& value) const
{
    if (value.typeId() == QMetaType::QString)
    {
        QString str = value.toString();
        str.replace("'", "''");
        return QString("'%1'").arg(str);
    }
    else if (value.typeId() == QMetaType::Bool)
    {
        return value.toBool() ? "1" : "0";
    }
    else if (value.typeId() == QMetaType::QByteArray)
    {
        QByteArray blob = value.toByteArray();
        return QString("X'%1'").arg(QString::fromLatin1(blob.toHex()));
    }
    else if (value.isNull())
    {
        return "NULL";
    }

    return value.toString();
}
