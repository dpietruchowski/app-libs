#pragma once

#include "platform/quickentrybackend.h"

class QuickEntry;

class AndroidQuickEntryBackend final : public QuickEntryBackend
{
public:
    explicit AndroidQuickEntryBackend(QuickEntry& owner);
    ~AndroidQuickEntryBackend() override;

    AndroidQuickEntryBackend(const AndroidQuickEntryBackend&) = delete;
    AndroidQuickEntryBackend& operator=(const AndroidQuickEntryBackend&) = delete;

    void configure(const QByteArray& json) override;
    bool canShow() const override;
    bool show() override;
    void hide() override;
    void setHeader(const QString& header) override;
    void startDelivery() override;
    void acknowledge(const QString& id) override;

private:
    QuickEntry& m_owner;
};
