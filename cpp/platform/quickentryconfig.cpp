#include "quickentryconfig.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

namespace
{

const QString valuePlaceholder = QStringLiteral("%1");

bool isResourceName(const QString& tone)
{
    static const QRegularExpression pattern(QStringLiteral("^[a-z][a-z0-9_]*$"));
    return pattern.match(tone).hasMatch();
}

QString variantsError(const QuickEntryMode& mode)
{
    QSet<QString> keys;
    for (const QuickEntryVariant& variant : mode.variants)
    {
        if (variant.key.isEmpty() || variant.label.isEmpty())
            return QStringLiteral("a variant of mode %1 has no key or no label").arg(mode.id);
        if (keys.contains(variant.key))
            return QStringLiteral("mode %1 repeats the variant %2").arg(mode.id, variant.key);
        keys.insert(variant.key);
    }
    if (!mode.initialVariant.isEmpty() && !keys.contains(mode.initialVariant))
        return QStringLiteral("mode %1 starts on the unknown variant %2")
            .arg(mode.id, mode.initialVariant);
    return {};
}

QString modeError(const QuickEntryMode& mode)
{
    if (mode.id.isEmpty() || mode.label.isEmpty())
        return QStringLiteral("a mode has no id or no label");
    if (!mode.tone.isEmpty() && !isResourceName(mode.tone))
        return QStringLiteral("the tone %1 of mode %2 is not a resource name")
            .arg(mode.tone, mode.id);
    if (mode.values.isEmpty())
        return QStringLiteral("mode %1 has no values").arg(mode.id);
    if (!mode.valueFormat.contains(valuePlaceholder))
        return QStringLiteral("the value format of mode %1 has no %2")
            .arg(mode.id, valuePlaceholder);
    if (!mode.headerFormat.contains(valuePlaceholder))
        return QStringLiteral("the header format of mode %1 has no %2")
            .arg(mode.id, valuePlaceholder);
    return variantsError(mode);
}

QJsonObject modeToJson(const QuickEntryMode& mode)
{
    QJsonArray values;
    for (int value : mode.values)
        values.append(value);

    QJsonArray variants;
    for (const QuickEntryVariant& variant : mode.variants)
        variants.append(QJsonObject { { "key", variant.key }, { "label", variant.label } });

    return {
        { "id", mode.id },
        { "label", mode.label },
        { "tone", mode.tone },
        { "valueFormat", mode.valueFormat },
        { "headerFormat", mode.headerFormat },
        { "initialValue", mode.initialValue },
        { "initialVariant", mode.initialVariant },
        { "values", values },
        { "variants", variants },
    };
}

}  // namespace

QString QuickEntryConfig::validationError() const
{
    if (channelId.isEmpty() || channelName.isEmpty())
        return QStringLiteral("the notification channel has no id or no name");
    if (modes.isEmpty())
        return QStringLiteral("there is no mode");

    QSet<QString> ids;
    for (const QuickEntryMode& mode : modes)
    {
        const QString error = modeError(mode);
        if (!error.isEmpty())
            return error;
        if (ids.contains(mode.id))
            return QStringLiteral("the mode %1 is repeated").arg(mode.id);
        ids.insert(mode.id);
    }
    return {};
}

QByteArray QuickEntryConfig::toJson() const
{
    QJsonArray modeObjects;
    for (const QuickEntryMode& mode : modes)
        modeObjects.append(modeToJson(mode));

    return QJsonDocument(QJsonObject {
                             { "channelId", channelId },
                             { "channelName", channelName },
                             { "smallIcon", smallIcon },
                             { "modes", modeObjects },
                         })
        .toJson(QJsonDocument::Compact);
}
