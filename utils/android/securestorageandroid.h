#pragma once

#include <QString>

class QSettings;

// Android Keystore backed secret storage.
//
// A hardware-backed AES/GCM key is generated once per service alias inside the
// Android Keystore and never leaves the TEE. Secrets are encrypted with it and
// only the ciphertext (+ IV) is persisted in app-private QSettings, so a copied
// database or settings file cannot be decrypted off-device.
class SecureStorageAndroid final
{
public:
    SecureStorageAndroid(const QString& organization, const QString& serviceName);

    bool isAvailable() const;
    bool contains(const QString& id) const;
    bool store(const QString& id, const QString& secret);
    QString retrieve(const QString& id) const;
    bool remove(const QString& id);

private:
    QSettings settings() const;
    QString settingsKey(const QString& id) const;

    QString m_organization;
    QString m_serviceName;
};
