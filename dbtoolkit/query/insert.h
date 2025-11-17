#pragma once
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "sqlcommand.h"

class Insert : public SqlCommand
{
public:
    Insert();

    Insert& into(const QString& table);
    Insert& columns(const QStringList& columns);
    Insert& values(const QVariantMap& values);
    Insert& value(const QString& column, const QVariant& value);

    QVariant execute(QSqlDatabase &database) const override;
    QString toSql() const override;

    bool hasTable() const;

private:
    QString m_table;
    QVariantMap m_values;
    QStringList m_columnOrder;
};
