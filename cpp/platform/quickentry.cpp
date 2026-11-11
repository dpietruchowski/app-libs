#include "quickentry.h"

#include <QDebug>
#include <QtGlobal>

#if defined(Q_OS_ANDROID)
#include "android/androidquickentry.h"
#else
class QuickEntryBackend
{
public:
    explicit QuickEntryBackend(QuickEntry&) { }

    void configure(const QByteArray&) { }
    bool canShow() const { return false; }
    bool show() { return false; }
    void hide() { }
    void setHeader(const QString&) { }
    void startDelivery() { }
    void acknowledge(const QString&) { }
};
#endif

QuickEntry::QuickEntry(QObject* parent)
    : QObject(parent)
    , m_backend(std::make_unique<QuickEntryBackend>(*this))
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
    if (m_header == header)
        return;

    m_header = header;
    m_backend->setHeader(header);
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
