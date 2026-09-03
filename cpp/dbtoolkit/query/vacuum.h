#pragma once
#include <QString>
#include <QVariant>

#include "sqlcommand.h"

class Vacuum : public SqlCommand
{
public:
    Vacuum();

    Vacuum& into(const QString& filePath);

    QString toSql() const override;
    QVariant execute(QSqlDatabase& database) const override;

private:
    QString m_filePath;
};
