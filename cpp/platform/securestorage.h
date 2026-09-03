#pragma once

#include <QString>

#include <memory>

// Cross-platform secret storage for small values like API keys.
//
// On Android the secret is encrypted with a hardware-backed key held in the
// Android Keystore (the key never leaves the TEE); only the ciphertext is
// persisted. The desktop backend is not implemented yet.
//
// `organization` is the host application's name (used as the QSettings
// organization), so this utility carries no app-specific identifiers.
class SecureStorage final
{
public:
    SecureStorage(const QString& organization, const QString& serviceName);
    ~SecureStorage();

    SecureStorage(const SecureStorage&) = delete;
    SecureStorage& operator=(const SecureStorage&) = delete;

    // Whether a secure backend is usable on this platform/device.
    bool isAvailable() const;

    bool contains(const QString& id) const;
    bool store(const QString& id, const QString& secret);
    QString retrieve(const QString& id) const;
    bool remove(const QString& id);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
