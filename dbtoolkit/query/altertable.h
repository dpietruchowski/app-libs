#pragma once
#include <QList>
#include <QString>

#include "column.h"
#include "sqlcommand.h"

// Builds `ALTER TABLE ... ADD COLUMN` statements. Idempotent on execute(): a
// column whose name already exists in the table is skipped (checked via the
// driver's table metadata, not a query), so the same migration is safe to run
// against databases at different schema states.
class AlterTable : public SqlCommand
{
public:
    explicit AlterTable(const QString& tableName);

    AlterTable& addColumn(const Column& col);

    QVariant execute(QSqlDatabase& database) const override;
    QString toSql() const override;

private:
    QString m_tableName;
    QList<ColumnDefinition> m_columns;
};
