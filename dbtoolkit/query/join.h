#pragma once
#include <QString>
#include <QStringList>

#include "alias.h"

class Join
{
public:
    explicit Join(const QString& tableName);

    Join& as(const QString& alias);
    Join& as(const TableAlias& alias);
    Join& on(const QString& leftAlias, const QString& leftColumn);
    Join& on(const TableAlias& leftAlias, const QString& leftColumn);
    Join& equals(const QString& rightColumn);
    Join& withColumns(const QStringList& columnKeys);
    Join& withPrefix(const QString& prefix);
    Join& withPrefix(const ColumnPrefix& prefix);

    QString tableName() const;
    QString tableAlias() const;
    QString tableWithAlias() const;
    QString condition() const;
    QStringList columnsWithPrefix() const;
    QString prefix() const;

private:
    QString m_tableName;
    TableAlias m_tableAlias;
    TableAlias m_leftAlias;
    QString m_leftColumn;
    QString m_rightColumn;
    QStringList m_columnKeys;
    std::optional<ColumnPrefix> m_prefix;
};
