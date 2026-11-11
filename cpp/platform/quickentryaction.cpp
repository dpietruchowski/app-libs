#include "quickentryaction.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

QList<QuickEntryAction> QuickEntryAction::listFromJson(const QByteArray& json)
{
    QList<QuickEntryAction> actions;
    const QJsonArray entries = QJsonDocument::fromJson(json).array();
    for (const QJsonValue& entry : entries)
    {
        const QJsonObject object = entry.toObject();
        const QString modeId = object.value("mode").toString();
        if (modeId.isEmpty())
            continue;

        actions.append({
            .modeId = modeId,
            .value = object.value("value").toInt(),
            .variantKey = object.value("variant").toString(),
            .time = QDateTime::fromMSecsSinceEpoch(object.value("time").toVariant().toLongLong()),
        });
    }
    return actions;
}
