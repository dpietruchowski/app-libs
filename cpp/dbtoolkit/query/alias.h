#pragma once

#include <QString>

class Alias
{
public:
    Alias() = default;
    
    QString createColumn(const QString& column) const;
    
    const QString& prefix() const { return m_prefix; }
    const QString& separator() const { return m_separator; }

protected:
    explicit Alias(const QString& prefix, const QString& separator);
    
    QString m_prefix;
    QString m_separator;
};

class TableAlias : public Alias
{
public:
    TableAlias() = default;
    explicit TableAlias(const QString& prefix);
};

class ColumnPrefix : public Alias
{
public:
    ColumnPrefix() = default;
    explicit ColumnPrefix(const QString& prefix);
};
