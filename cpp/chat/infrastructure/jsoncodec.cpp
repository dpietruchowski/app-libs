#include "chat/infrastructure/jsoncodec.h"

#include <QJsonObject>
#include <QString>

#include "dbtoolkit/sqlcodec.h"

namespace chat::json_codec
{

namespace
{
    const QString kKey = QStringLiteral("key");
    const QString kTitle = QStringLiteral("title");
    const QString kData = QStringLiteral("data");
}

QVariant contextToSql(const ChatContext& context)
{
    QJsonObject object;
    object.insert(kKey, context.key);
    object.insert(kTitle, context.title);
    object.insert(kData, context.data);
    return sql_codec::jsonObjectToSql(object);
}

std::optional<ChatContext> contextFromSql(const QVariant& value)
{
    if (value.isNull())
    {
        return std::nullopt;
    }
    const QJsonObject object = sql_codec::jsonObjectFromSql(value);
    return ChatContext { object.value(kKey).toString(), object.value(kTitle).toString(),
                         object.value(kData).toObject() };
}

}
