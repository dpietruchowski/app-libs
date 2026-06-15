#pragma once
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QVector>
#include <optional>

#include "sqlcommand.h"

class Pragma : public SqlCommand
{
public:
    explicit Pragma(const QString& name);

    Pragma& withArgument(const QString& argument);
    Pragma& set(const QVariant& value);

    QString toSql() const override;
    QVariant execute(QSqlDatabase& database) const override;
    QVector<QVariantMap> query(QSqlDatabase& database) const;

private:
    QString m_name;
    QString m_argument;
    std::optional<QVariant> m_value;

    QString formatValue(const QVariant& value) const;
};
