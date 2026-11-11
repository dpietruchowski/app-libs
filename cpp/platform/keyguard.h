#pragma once

#include <QObject>

#include <memory>

class KeyguardBackend;

class Keyguard final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool locked READ locked NOTIFY stateChanged)
    Q_PROPERTY(bool shownOverLock READ shownOverLock NOTIFY stateChanged)

public:
    explicit Keyguard(QObject* parent = nullptr);
    ~Keyguard() override;

    bool locked() const { return m_locked; }
    bool shownOverLock() const { return m_shownOverLock; }

    Q_INVOKABLE void requestUnlock();

    void update(bool locked, bool shownOverLock);
    void finishUnlock(bool succeeded);

signals:
    void stateChanged();
    void unlockFinished(bool succeeded);

private:
    bool m_locked = false;
    bool m_shownOverLock = false;
    std::unique_ptr<KeyguardBackend> m_backend;
};
