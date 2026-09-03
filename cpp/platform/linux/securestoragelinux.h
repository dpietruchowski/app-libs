#pragma once

#include <QString>

// libsecret (Secret Service) backed secret storage for Linux desktops.
//
// Secrets are stored by the session secret service daemon (gnome-keyring /
// KWallet), encrypted with a key derived from the user's login. The application
// never holds the encryption key; access is gated by the user session.
class SecureStorageLinux final
{
public:
    SecureStorageLinux(const QString& organization, const QString& serviceName);

    bool isAvailable() const;
    bool contains(const QString& id) const;
    bool store(const QString& id, const QString& secret);
    QString retrieve(const QString& id) const;
    bool remove(const QString& id);

private:
    QString m_service;
};
