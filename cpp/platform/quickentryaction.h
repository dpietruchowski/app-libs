#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>

struct QuickEntryAction
{
    QString id;
    QString modeId;
    int value { 0 };
    QString variantKey;
    QDateTime time;

    friend bool operator==(const QuickEntryAction&, const QuickEntryAction&) = default;

    static QList<QuickEntryAction> listFromJson(const QByteArray& json);
};
