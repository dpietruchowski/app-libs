#include "column.h"

QString ColumnDefinition::toSql() const
{
    QString sql = name + " ";
    
    switch (type)
    {
    case ColumnType::Integer:
        sql += "INTEGER";
        break;
    case ColumnType::Real:
        sql += "REAL";
        break;
    case ColumnType::Text:
        sql += "TEXT";
        break;
    case ColumnType::Blob:
        sql += "BLOB";
        break;
    case ColumnType::Numeric:
        sql += "NUMERIC";
        break;
    case ColumnType::Boolean:
        sql += "BOOLEAN";
        break;
    case ColumnType::DateTime:
        sql += "DATETIME";
        break;
    }
    
    if (primaryKey)
    {
        sql += " PRIMARY KEY";
    }
    
    if (autoIncrement)
    {
        sql += " AUTOINCREMENT";
    }
    
    if (notNull)
    {
        sql += " NOT NULL";
    }
    
    if (unique)
    {
        sql += " UNIQUE";
    }
    
    if (!defaultValue.isNull())
    {
        if (defaultValue.typeId() == QMetaType::QString)
        {
            QString strValue = defaultValue.toString();
            if (strValue.startsWith("(") && strValue.endsWith(")"))
            {
                sql += " DEFAULT " + strValue;
            }
            else
            {
                sql += " DEFAULT '" + strValue + "'";
            }
        }
        else
        {
            sql += " DEFAULT " + defaultValue.toString();
        }
    }
    
    return sql;
}

Column::Column(const QString& name)
{
    m_definition.name = name;
}

Column& Column::integer()
{
    m_definition.type = ColumnType::Integer;
    return *this;
}

Column& Column::real()
{
    m_definition.type = ColumnType::Real;
    return *this;
}

Column& Column::text()
{
    m_definition.type = ColumnType::Text;
    return *this;
}

Column& Column::blob()
{
    m_definition.type = ColumnType::Blob;
    return *this;
}

Column& Column::numeric()
{
    m_definition.type = ColumnType::Numeric;
    return *this;
}

Column& Column::boolean()
{
    m_definition.type = ColumnType::Boolean;
    return *this;
}

Column& Column::dateTime()
{
    m_definition.type = ColumnType::DateTime;
    return *this;
}

Column& Column::primaryKey()
{
    m_definition.primaryKey = true;
    return *this;
}

Column& Column::autoIncrement()
{
    m_definition.autoIncrement = true;
    return *this;
}

Column& Column::notNull()
{
    m_definition.notNull = true;
    return *this;
}

Column& Column::unique()
{
    m_definition.unique = true;
    return *this;
}

Column& Column::defaultValue(const QVariant& value)
{
    m_definition.defaultValue = value;
    return *this;
}

Column& Column::datetime()
{
    m_definition.type = ColumnType::DateTime;
    return *this;
}

QString Column::toSql() const
{
    return m_definition.toSql();
}
