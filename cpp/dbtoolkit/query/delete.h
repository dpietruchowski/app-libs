#pragma once
#include <QString>

#include "sqlcommand.h"
#include "where.h"

class Delete : public SqlCommand
{
public:
    Delete();
    explicit Delete(const QString& table);

    Delete& from(const QString& table);
    Delete& where(const Where& condition);
    Delete& where(const QString& condition);

    Delete& all();

    QVariant execute(QSqlDatabase &database) const override;
    QString toSql() const override;
    QString build() const override;

    bool hasTable() const;
    bool hasWhere() const;

private:
    bool m_deleteAll = false;
    QString m_table;
    QString m_where;
};
