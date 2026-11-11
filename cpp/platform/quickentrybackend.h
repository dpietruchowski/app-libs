#pragma once

#include <QByteArray>
#include <QString>

class QuickEntryBackend
{
public:
    virtual ~QuickEntryBackend() = default;

    virtual void configure(const QByteArray& json) = 0;
    virtual bool canShow() const = 0;
    virtual bool show() = 0;
    virtual void hide() = 0;
    virtual void setHeader(const QString& header) = 0;
    virtual void startDelivery() = 0;
    virtual void acknowledge(const QString& id) = 0;
};
