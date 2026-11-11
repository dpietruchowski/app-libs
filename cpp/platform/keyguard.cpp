#include "keyguard.h"

#include <QtGlobal>

#if defined(Q_OS_ANDROID)
#include "android/androidkeyguard.h"
#else
class KeyguardBackend
{
public:
    explicit KeyguardBackend(Keyguard& owner)
        : m_owner(owner)
    {
    }

    void requestUnlock()
    {
        m_owner.update(false, false);
        m_owner.finishUnlock(true);
    }

    void returnToLockScreen() { m_owner.update(m_owner.locked(), false); }

private:
    Keyguard& m_owner;
};
#endif

Keyguard::Keyguard(QObject* parent)
    : QObject(parent)
    , m_backend(std::make_unique<KeyguardBackend>(*this))
{
}

Keyguard::~Keyguard() = default;

void Keyguard::requestUnlock() { m_backend->requestUnlock(); }

void Keyguard::returnToLockScreen() { m_backend->returnToLockScreen(); }

void Keyguard::update(bool locked, bool shownOverLock)
{
    if (m_locked == locked && m_shownOverLock == shownOverLock)
        return;

    m_locked = locked;
    m_shownOverLock = shownOverLock;
    emit stateChanged();
}

void Keyguard::finishUnlock(bool succeeded) { emit unlockFinished(succeeded); }
