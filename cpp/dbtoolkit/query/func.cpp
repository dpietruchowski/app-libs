#include "func.h"

Func::Func(const QString& name, const QStringList& arguments)
    : m_name(name)
    , m_arguments(arguments)
{
}

Func Func::coalesce(const QString& expression, const QString& fallback)
{
    return Func("COALESCE", { expression, fallback });
}

Func Func::max(const QStringList& arguments) { return Func("MAX", arguments); }

Func Func::min(const QStringList& arguments) { return Func("MIN", arguments); }

Func Func::count(const QString& expression) { return Func("COUNT", { expression }); }

Func Func::countDistinct(const QString& expression)
{
    return Func("COUNT", { QString("DISTINCT %1").arg(expression) });
}

QString Func::toSql() const { return QString("%1(%2)").arg(m_name, m_arguments.join(", ")); }
