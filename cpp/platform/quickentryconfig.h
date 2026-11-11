#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

struct QuickEntryVariant
{
    QString key;
    QString label;

    friend bool operator==(const QuickEntryVariant&, const QuickEntryVariant&) = default;
};

struct QuickEntryMode
{
    QString id;
    QString label;
    QList<int> values;
    int initialValue { 0 };
    QString initialVariant;
    QString valueFormat { QStringLiteral("%1") };
    QString headerFormat { QStringLiteral("%1") };
    QList<QuickEntryVariant> variants;

    friend bool operator==(const QuickEntryMode&, const QuickEntryMode&) = default;
};

struct QuickEntryConfig
{
    QString channelId { QStringLiteral("quick_entry") };
    QString channelName { QStringLiteral("Quick entry") };
    QString smallIcon { QStringLiteral("ic_notification") };
    QList<QuickEntryMode> modes;

    friend bool operator==(const QuickEntryConfig&, const QuickEntryConfig&) = default;

    QString validationError() const;
    QByteArray toJson() const;
};
