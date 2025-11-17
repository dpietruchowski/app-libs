#pragma once
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

#include "column.h"
#include "sqlcommand.h"

enum class OnDeleteAction
{
    NoAction,
    Cascade,
    SetNull,
    Restrict,
    SetDefault
};

struct ForeignKeyDefinition
{
    QString column;
    QString referencedTable;
    QString referencedColumn;
    OnDeleteAction onDelete;

    QString toSql() const;
};

struct UniqueConstraint
{
    QStringList columns;
    
    QString toSql() const;
};

class CreateTable : public SqlCommand
{
public:
    explicit CreateTable(const QString& tableName);
    
    CreateTable& column(const Column& col);
    CreateTable& ifNotExists();

    CreateTable& foreignKey(const QString& column, const QString& referencedTable,
                            const QString& referencedColumn,
                            OnDeleteAction onDelete = OnDeleteAction::NoAction);
    CreateTable& uniqueConstraint(const QStringList& columns);

    QVariant execute(QSqlDatabase &database) const override;
    QString toSql() const override;
    QString build() const override;

private:
    QString m_tableName;
    QList<ColumnDefinition> m_columns;
    QList<ForeignKeyDefinition> m_foreignKeys;
    QList<UniqueConstraint> m_uniqueConstraints;
    bool m_ifNotExists = false;
};
