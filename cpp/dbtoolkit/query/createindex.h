#pragma once
#include <QString>
#include <QStringList>
#include <optional>

#include "sqlcommand.h"
#include "where.h"

class CreateIndex : public SqlCommand
{
public:
    explicit CreateIndex(const QString& indexName, const QString& tableName);

    CreateIndex& unique();
    CreateIndex& ifNotExists();
    CreateIndex& columns(const QStringList& cols);
    CreateIndex& where(const Where& predicate);

    QString toSql() const override;
    QString build() const override;
    QVariant execute(QSqlDatabase& db) const override;

private:
    QString m_indexName;
    QString m_tableName;
    QStringList m_columns;
    std::optional<Where> m_where;
    bool m_unique = false;
    bool m_ifNotExists = false;
};
