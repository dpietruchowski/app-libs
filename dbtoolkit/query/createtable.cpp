#include "createtable.h"
#include <QSqlError>
#include <QSqlQuery>
#include <QDebug>

namespace
{
QString onDeleteActionToString(OnDeleteAction action)
{
    switch (action)
    {
        case OnDeleteAction::NoAction:
            return "";
        case OnDeleteAction::Cascade:
            return "CASCADE";
        case OnDeleteAction::SetNull:
            return "SET NULL";
        case OnDeleteAction::Restrict:
            return "RESTRICT";
        case OnDeleteAction::SetDefault:
            return "SET DEFAULT";
    }
    return "";
}
}

QString ForeignKeyDefinition::toSql() const
{
    QString sql = QString("FOREIGN KEY(%1) REFERENCES %2(%3)")
                      .arg(column)
                      .arg(referencedTable)
                      .arg(referencedColumn);

    QString onDeleteStr = onDeleteActionToString(onDelete);
    if (!onDeleteStr.isEmpty())
    {
        sql += " ON DELETE " + onDeleteStr;
    }

    return sql;
}

QString UniqueConstraint::toSql() const
{
    return QString("UNIQUE(%1)").arg(columns.join(", "));
}

CreateTable::CreateTable(const QString& tableName)
    : m_tableName(tableName)
{
}

CreateTable& CreateTable::column(const Column& col)
{
    m_columns.append(col.definition());
    return *this;
}

CreateTable& CreateTable::ifNotExists()
{
    m_ifNotExists = true;
    return *this;
}

CreateTable& CreateTable::foreignKey(const QString& column, const QString& referencedTable,
                                     const QString& referencedColumn, OnDeleteAction onDelete)
{
    ForeignKeyDefinition fk;
    fk.column = column;
    fk.referencedTable = referencedTable;
    fk.referencedColumn = referencedColumn;
    fk.onDelete = onDelete;
    m_foreignKeys.append(fk);
    return *this;
}

CreateTable& CreateTable::uniqueConstraint(const QStringList& columns)
{
    UniqueConstraint uc;
    uc.columns = columns;
    m_uniqueConstraints.append(uc);
    return *this;
}

int CreateTable::execute(QSqlDatabase& database) const
{
    QString sql = toSql();
    
    QSqlQuery query(database);
    if (!query.exec(sql))
    {
        qWarning() << "CreateTable exec failed:" << query.lastError();
        qWarning() << "SQL:" << sql;
        return 0;
    }
    
    return 1;
}

QString CreateTable::toSql() const
{
    if (m_tableName.isEmpty() || m_columns.isEmpty())
    {
        return QString();
    }
    
    QString sql = "CREATE TABLE";
    
    if (m_ifNotExists)
    {
        sql += " IF NOT EXISTS";
    }
    
    sql += " " + m_tableName + " (";
    
    QStringList parts;
    
    for (const auto& col : m_columns)
    {
        parts.append(col.toSql());
    }
    
    for (const auto& fk : m_foreignKeys)
    {
        parts.append(fk.toSql());
    }
    
    for (const auto& uc : m_uniqueConstraints)
    {
        parts.append(uc.toSql());
    }
    
    sql += parts.join(", ");
    sql += ")";
    
    return sql;
}

QString CreateTable::build() const
{
    return toSql();
}
