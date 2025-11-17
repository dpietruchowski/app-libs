#pragma once
#include <QString>
#include <QVariantMap>

#include "sqlcommand.h"
#include "where.h"

class Update : public SqlCommand
{
public:
    Update();
    explicit Update(const QString& table);

    Update& table(const QString& table);
    Update& set(const QVariantMap& values);
    Update& set(const QString& column, const QVariant& value);
    Update& where(const Where& condition);
    Update& where(const QString& condition);

    QVariant execute(QSqlDatabase &database) const override;
    QString toSql() const override;
    QString build() const override;

    bool hasTable() const;
    bool hasWhere() const;

private:
    QString m_table;
    QVariantMap m_values;
    QString m_where;
};
