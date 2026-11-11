#pragma once

class Keyguard;

class KeyguardBackend final
{
public:
    explicit KeyguardBackend(Keyguard& owner);
    ~KeyguardBackend();

    KeyguardBackend(const KeyguardBackend&) = delete;
    KeyguardBackend& operator=(const KeyguardBackend&) = delete;

    void requestUnlock();
    void returnToLockScreen();

private:
    Keyguard& m_owner;
};
