#pragma once

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QString>
#include <QUuid>
#include <QVariant>

#include <type_traits>
#include <vector>

namespace sql_codec
{

inline QVariant toSql(const QUuid& id)
{
    return id.isNull() ? QVariant() : QVariant(id.toRfc4122());
}

inline QVariantList toSqlList(const std::vector<QUuid>& ids)
{
    QVariantList list;
    list.reserve(static_cast<int>(ids.size()));
    for (const QUuid& id : ids)
    {
        list.append(toSql(id));
    }
    return list;
}

inline QUuid uuidFromSql(const QVariant& value)
{
    return value.isNull() ? QUuid() : QUuid::fromRfc4122(value.toByteArray());
}

inline QVariant toSql(const QDateTime& dateTime)
{
    return dateTime.isValid() ? QVariant(dateTime.toUTC().toString(Qt::ISODateWithMs)) : QVariant();
}

inline QDateTime dateTimeFromSql(const QVariant& value)
{
    return value.isNull() ? QDateTime()
                          : QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
}

inline QVariant toSql(const QDate& date)
{
    return date.isValid() ? QVariant(date.toString(Qt::ISODate)) : QVariant();
}

inline QDate dateFromSql(const QVariant& value)
{
    return value.isNull() ? QDate() : QDate::fromString(value.toString(), Qt::ISODate);
}

inline QVariant toSql(bool value) { return QVariant(value ? 1 : 0); }

inline QVariant toSql(const QString& text) { return QVariant(text.isNull() ? QString("") : text); }

inline bool boolFromSql(const QVariant& value) { return value.toInt() != 0; }

inline QVariant toSql(const std::vector<QString>& strings)
{
    QJsonArray array;
    for (const QString& text : strings)
    {
        array.append(text);
    }
    return QVariant(QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)));
}

inline std::vector<QString> stringListFromSql(const QVariant& value)
{
    std::vector<QString> strings;
    const QJsonArray array = QJsonDocument::fromJson(value.toString().toUtf8()).array();
    strings.reserve(array.size());
    for (const QJsonValue& item : array)
    {
        strings.push_back(item.toString());
    }
    return strings;
}

template <typename E>
    requires std::is_enum_v<E>
QVariant toSql(E value)
{
    return QVariant(static_cast<int>(value));
}

template <typename E>
    requires std::is_enum_v<E>
E enumFromSql(const QVariant& value)
{
    return static_cast<E>(value.toInt());
}

}
