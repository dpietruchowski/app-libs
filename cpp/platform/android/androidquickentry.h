#pragma once

#include <QByteArray>
#include <QString>

class QuickEntry;

class QuickEntryBackend final
{
public:
    explicit QuickEntryBackend(QuickEntry& owner);
    ~QuickEntryBackend();

    QuickEntryBackend(const QuickEntryBackend&) = delete;
    QuickEntryBackend& operator=(const QuickEntryBackend&) = delete;

    void configure(const QByteArray& json);
    bool canShow() const;
    bool show();
    void hide();
    void setHeader(const QString& header);
    void startDelivery();
    void acknowledge(const QString& id);

private:
    QuickEntry& m_owner;
};
