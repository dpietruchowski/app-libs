#include "quickentry.h"

#include <QDebug>
#include <QtGlobal>
#include <utility>

#if defined(Q_OS_ANDROID)
#include "android/androidquickentry.h"
using PlatformQuickEntryBackend = AndroidQuickEntryBackend;
#else
class NullQuickEntryBackend final : public QuickEntryBackend
{
public:
    explicit NullQuickEntryBackend(QuickEntry&) { }

    void configure(const QByteArray&) override { }
    bool canShow() const override { return false; }
    bool show() override { return false; }
    void hide() override { }
    void setHeader(const QString&) override { }
    void startDelivery() override { }
    void acknowledge(const QString&) override { }
};
using PlatformQuickEntryBackend = NullQuickEntryBackend;
#endif

QuickEntry::QuickEntry(QObject* parent)
    : QObject(parent)
    , m_backend(std::make_unique<PlatformQuickEntryBackend>(*this))
{
}

QuickEntry::QuickEntry(std::unique_ptr<QuickEntryBackend> backend, QObject* parent)
    : QObject(parent)
    , m_backend(std::move(backend))
{
}

QuickEntry::~QuickEntry() = default;

bool QuickEntry::configure(const QuickEntryConfig& config)
{
    const QString error = config.validationError();
    if (!error.isEmpty())
    {
        qWarning() << "Quick entry configuration rejected:" << error;
        return false;
    }

    m_backend->configure(config.toJson());
    m_configured = true;
    return true;
}

bool QuickEntry::canShow() const { return m_configured && m_backend->canShow(); }

bool QuickEntry::show()
{
    if (!canShow() || !m_backend->show())
        return false;

    if (!m_visible)
    {
        m_visible = true;
        emit visibleChanged();
    }
    return true;
}

void QuickEntry::hide()
{
    m_backend->hide();
    if (m_visible)
    {
        m_visible = false;
        emit visibleChanged();
    }
}

void QuickEntry::setHeader(const QString& header)
{
    m_backend->setHeader(header);

    if (m_header == header)
        return;

    m_header = header;
    emit headerChanged();
}

void QuickEntry::startDelivery()
{
    m_deliveredIds.clear();
    m_backend->startDelivery();
}

void QuickEntry::acknowledge(const QString& id) { m_backend->acknowledge(id); }

void QuickEntry::deliver(const QList<QuickEntryAction>& pending)
{
    QSet<QString> pendingIds;
    QList<QuickEntryAction> undelivered;
    for (const QuickEntryAction& action : pending)
    {
        pendingIds.insert(action.id);
        if (!m_deliveredIds.contains(action.id))
            undelivered.append(action);
    }
    m_deliveredIds = pendingIds;

    for (const QuickEntryAction& action : undelivered)
        emit committed(action);
}
