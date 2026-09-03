#include "stringtemplate.h"

#include <QRegularExpression>

StringTemplate::StringTemplate(QString source)
    : m_source(std::move(source))
{
}

QString StringTemplate::render(const QVariantMap& values) const
{
    static const QRegularExpression placeholder(QStringLiteral("\\{\\{\\s*([A-Za-z0-9_]+)\\s*\\}\\}"));

    QString result;
    result.reserve(m_source.size());

    int cursor = 0;
    auto it = placeholder.globalMatch(m_source);
    while (it.hasNext())
    {
        auto match = it.next();
        result.append(m_source.mid(cursor, match.capturedStart() - cursor));
        const QString key = match.captured(1);
        result.append(values.value(key).toString());
        cursor = match.capturedEnd();
    }
    result.append(m_source.mid(cursor));
    return result;
}

const QString& StringTemplate::source() const { return m_source; }
