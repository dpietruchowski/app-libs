#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

struct QuickEntryVariant
{
    QString key;
    QString label;
};

struct QuickEntryMode
{
    QString id;
    QString label;
    QList<int> values;
    int initialValue { 0 };
    QString valueFormat { QStringLiteral("%1") };
    QString headerFormat { QStringLiteral("%1") };
    QList<QuickEntryVariant> variants;
};

struct QuickEntryConfig
{
    QString channelId { QStringLiteral("quick_entry") };
    QString channelName { QStringLiteral("Quick entry") };
    QString smallIcon { QStringLiteral("ic_notification") };
    QList<QuickEntryMode> modes;

    QString validationError() const;
    QByteArray toJson() const;
};
