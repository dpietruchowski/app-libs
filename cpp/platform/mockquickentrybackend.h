#pragma once

#include "platform/quickentrybackend.h"

#include <QByteArray>
#include <QList>
#include <QStringList>

class MockQuickEntryBackend final : public QuickEntryBackend
{
public:
    void configure(const QByteArray& json) override { configurations.append(json); }
    bool canShow() const override { return canShowResult; }

    bool show() override
    {
        ++showCalls;
        return canShowResult;
    }

    void hide() override { ++hideCalls; }
    void setHeader(const QString& header) override { headers.append(header); }
    void startDelivery() override { ++deliveryStarts; }
    void acknowledge(const QString& id) override { acknowledged.append(id); }

    bool canShowResult = true;
    QList<QByteArray> configurations;
    int showCalls = 0;
    int hideCalls = 0;
    QStringList headers;
    int deliveryStarts = 0;
    QStringList acknowledged;
};
