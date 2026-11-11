#pragma once

#include "platform/quickentryaction.h"
#include "platform/quickentryconfig.h"

#include <QObject>

#include <memory>

class QuickEntryBackend;

class QuickEntry final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
    Q_PROPERTY(QString header READ header WRITE setHeader NOTIFY headerChanged)

public:
    explicit QuickEntry(QObject* parent = nullptr);
    ~QuickEntry() override;

    bool configure(const QuickEntryConfig& config);
    bool canShow() const;
    bool visible() const { return m_visible; }
    QString header() const { return m_header; }

    Q_INVOKABLE bool show();
    Q_INVOKABLE void hide();
    void setHeader(const QString& header);

    void startDelivery();
    void deliver(const QuickEntryAction& action);

signals:
    void visibleChanged();
    void headerChanged();
    void committed(const QuickEntryAction& action);

private:
    bool m_configured = false;
    bool m_visible = false;
    QString m_header;
    std::unique_ptr<QuickEntryBackend> m_backend;
};
