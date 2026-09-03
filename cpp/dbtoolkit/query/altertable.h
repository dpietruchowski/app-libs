#pragma once
#include <QList>
#include <QString>
#include <QStringList>

#include "column.h"
#include "sqlcommand.h"

// Builds `ALTER TABLE ... ADD COLUMN` / `DROP COLUMN` statements. Idempotent on
// execute(): an added column whose name already exists is skipped and a dropped
// column that is absent is skipped (both checked via the driver's table
// metadata, not a query), so the same migration is safe to run against
// databases at different schema states.
class AlterTable : public SqlCommand
{
public:
    explicit AlterTable(const QString& tableName);

    AlterTable& addColumn(const Column& col);
    AlterTable& dropColumn(const QString& name);

    QVariant execute(QSqlDatabase& database) const override;
    QString toSql() const override;

private:
    QString m_tableName;
    QList<ColumnDefinition> m_columns;
    QStringList m_dropColumns;
};
