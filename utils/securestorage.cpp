#include "securestorage.h"

#if defined(Q_OS_ANDROID)
#include "android/securestorageandroid.h"
#define SECURE_STORAGE_BACKEND SecureStorageAndroid
#elif defined(HAVE_LIBSECRET)
#include "linux/securestoragelinux.h"
#define SECURE_STORAGE_BACKEND SecureStorageLinux
#endif

class SecureStorage::Impl
{
public:
    Impl(const QString& organization, const QString& serviceName)
#ifdef SECURE_STORAGE_BACKEND
        : backend(organization, serviceName)
#endif
    {
#ifndef SECURE_STORAGE_BACKEND
        Q_UNUSED(organization);
        Q_UNUSED(serviceName);
#endif
    }

#ifdef SECURE_STORAGE_BACKEND
    SECURE_STORAGE_BACKEND backend;
#endif
};

SecureStorage::SecureStorage(const QString& organization, const QString& serviceName)
    : m_impl(std::make_unique<Impl>(organization, serviceName))
{
}

SecureStorage::~SecureStorage() = default;

bool SecureStorage::isAvailable() const
{
#ifdef SECURE_STORAGE_BACKEND
    return m_impl->backend.isAvailable();
#else
    return false;
#endif
}

bool SecureStorage::contains(const QString& id) const
{
#ifdef SECURE_STORAGE_BACKEND
    return m_impl->backend.contains(id);
#else
    Q_UNUSED(id);
    return false;
#endif
}

bool SecureStorage::store(const QString& id, const QString& secret)
{
#ifdef SECURE_STORAGE_BACKEND
    return m_impl->backend.store(id, secret);
#else
    Q_UNUSED(id);
    Q_UNUSED(secret);
    return false;
#endif
}

QString SecureStorage::retrieve(const QString& id) const
{
#ifdef SECURE_STORAGE_BACKEND
    return m_impl->backend.retrieve(id);
#else
    Q_UNUSED(id);
    return QString();
#endif
}

bool SecureStorage::remove(const QString& id)
{
#ifdef SECURE_STORAGE_BACKEND
    return m_impl->backend.remove(id);
#else
    Q_UNUSED(id);
    return false;
#endif
}
