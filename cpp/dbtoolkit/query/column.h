#pragma once
#include <QString>
#include <QVariant>

enum class ColumnType
{
    Integer,
    Real,
    Text,
    Blob,
    Numeric,
    Boolean,
    DateTime
};

struct ColumnDefinition
{
    QString name;
    ColumnType type = ColumnType::Text;
    bool primaryKey = false;
    bool autoIncrement = false;
    bool notNull = false;
    bool unique = false;
    QVariant defaultValue;
    
    QString toSql() const;
};

class Column
{
public:
    explicit Column(const QString& name);
    
    Column& integer();
    Column& real();
    Column& text();
    Column& blob();
    Column& numeric();
    Column& boolean();
    Column& dateTime();
    
    Column& primaryKey();
    Column& autoIncrement();
    Column& notNull();
    Column& unique();
    Column& defaultValue(const QVariant& value);
    Column& datetime();

    QString toSql() const;
    const ColumnDefinition& definition() const { return m_definition; }
    
private:
    ColumnDefinition m_definition;
};
