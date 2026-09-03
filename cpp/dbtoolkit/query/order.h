#pragma once
#include <QString>
#include <QStringList>

#include "alias.h"

class Order
{
public:
    Order();
    explicit Order(const QString& column);
    explicit Order(const QString& alias, const QString& column);
    explicit Order(const TableAlias& alias, const QString& column);

    Order& asc();
    Order& desc();
    Order& then(const QString& column);
    Order& then(const QString& alias, const QString& column);
    Order& then(const TableAlias& alias, const QString& column);

    QString build() const;
    bool isEmpty() const;

private:
    QStringList m_orderClauses;
    QString m_currentColumn;
    
    void finalizeCurrent(const QString& direction);
};
