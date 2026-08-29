#include "vacuum.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlQuery>

Vacuum::Vacuum() = default;

Vacuum& Vacuum::into(const QString& filePath)
{
    m_filePath = filePath;
    return *this;
}

QString Vacuum::toSql() const
{
    if (m_filePath.isEmpty())
    {
        return "VACUUM";
    }

    return "VACUUM INTO ?";
}

QVariant Vacuum::execute(QSqlDatabase& database) const
{
    QString sql = toSql();

    QSqlQuery query(database);
    if (!query.prepare(sql))
    {
        qWarning() << "Vacuum prepare failed:" << query.lastError();
        qWarning() << "SQL:" << sql;
        return 0;
    }

    if (!m_filePath.isEmpty())
    {
        query.addBindValue(m_filePath);
    }

    if (!query.exec())
    {
        qWarning() << "Vacuum exec failed:" << query.lastError();
        qWarning() << "SQL:" << sql << "into:" << m_filePath;
        return 0;
    }

    return 1;
}
