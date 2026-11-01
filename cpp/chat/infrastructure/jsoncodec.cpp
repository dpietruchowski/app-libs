#include "chat/infrastructure/jsoncodec.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QString>

namespace chat::json_codec
{

namespace
{
    const QString kKey = QStringLiteral("key");
    const QString kTitle = QStringLiteral("title");
    const QString kData = QStringLiteral("data");

    QString compactJson(const QJsonDocument& document)
    {
        return QString::fromUtf8(document.toJson(QJsonDocument::Compact));
    }
}

QVariant objectToSql(const QJsonObject& object) { return compactJson(QJsonDocument(object)); }

QJsonObject objectFromSql(const QVariant& value)
{
    return QJsonDocument::fromJson(value.toString().toUtf8()).object();
}

QVariant valueToSql(const QJsonValue& value)
{
    if (value.isUndefined())
    {
        return QVariant();
    }
    QJsonArray wrapper;
    wrapper.append(value);
    return compactJson(QJsonDocument(wrapper));
}

QJsonValue valueFromSql(const QVariant& value)
{
    if (value.isNull())
    {
        return QJsonValue(QJsonValue::Undefined);
    }
    const QJsonArray wrapper = QJsonDocument::fromJson(value.toString().toUtf8()).array();
    return wrapper.isEmpty() ? QJsonValue(QJsonValue::Undefined) : wrapper.first();
}

QVariant contextToSql(const ChatContext& context)
{
    QJsonObject object;
    object.insert(kKey, context.key);
    object.insert(kTitle, context.title);
    object.insert(kData, context.data);
    return objectToSql(object);
}

std::optional<ChatContext> contextFromSql(const QVariant& value)
{
    if (value.isNull())
    {
        return std::nullopt;
    }
    const QJsonObject object = objectFromSql(value);
    return ChatContext { object.value(kKey).toString(), object.value(kTitle).toString(),
                         object.value(kData).toObject() };
}

}
