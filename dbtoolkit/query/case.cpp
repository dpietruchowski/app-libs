#include "case.h"

Case::Case(const QString& subject)
    : m_subject(subject)
{
}

Case& Case::when(const QVariant& match, const QVariant& result)
{
    m_branches.append({ match, result });
    return *this;
}

Case& Case::otherwise(const QString& rawExpression)
{
    m_else = rawExpression;
    return *this;
}

QString Case::toSql() const
{
    QString sql = QString("CASE %1").arg(m_subject);
    for (const auto& branch : m_branches)
    {
        sql += QString(" WHEN %1 THEN %2").arg(formatValue(branch.first), formatValue(branch.second));
    }
    if (!m_else.isEmpty())
    {
        sql += QString(" ELSE %1").arg(m_else);
    }
    sql += " END";
    return sql;
}

QString Case::formatValue(const QVariant& value) const
{
    if (value.typeId() == QMetaType::QString)
    {
        QString str = value.toString();
        str.replace("'", "''");
        return QString("'%1'").arg(str);
    }
    if (value.isNull())
    {
        return "NULL";
    }
    return value.toString();
}
