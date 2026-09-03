#include "securestoragelinux.h"

#include <libsecret/secret.h>

namespace
{
const char* kAttributeService = "service";
const char* kAttributeId = "id";

const SecretSchema* schema()
{
    static const SecretSchema instance = {
        "com.fillin.SecureStorage",
        SECRET_SCHEMA_NONE,
        {
            { kAttributeService, SECRET_SCHEMA_ATTRIBUTE_STRING },
            { kAttributeId, SECRET_SCHEMA_ATTRIBUTE_STRING },
            { nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING },
        },
    };
    return &instance;
}
}

SecureStorageLinux::SecureStorageLinux(const QString& organization, const QString& serviceName)
    : m_service(organization + QLatin1Char('.') + serviceName)
{
}

bool SecureStorageLinux::isAvailable() const
{
    GError* error = nullptr;
    SecretService* service = secret_service_get_sync(SECRET_SERVICE_NONE, nullptr, &error);
    if (error)
    {
        g_error_free(error);
        return false;
    }
    if (!service)
    {
        return false;
    }
    g_object_unref(service);
    return true;
}

bool SecureStorageLinux::contains(const QString& id) const { return !retrieve(id).isEmpty(); }

bool SecureStorageLinux::store(const QString& id, const QString& secret)
{
    const QByteArray service = m_service.toUtf8();
    const QByteArray key = id.toUtf8();
    const QByteArray value = secret.toUtf8();
    const QByteArray label = (m_service + QLatin1Char('/') + id).toUtf8();

    GError* error = nullptr;
    const gboolean ok = secret_password_store_sync(
        schema(), SECRET_COLLECTION_DEFAULT, label.constData(), value.constData(), nullptr, &error,
        kAttributeService, service.constData(), kAttributeId, key.constData(), nullptr);
    if (error)
    {
        g_error_free(error);
        return false;
    }
    return ok;
}

QString SecureStorageLinux::retrieve(const QString& id) const
{
    const QByteArray service = m_service.toUtf8();
    const QByteArray key = id.toUtf8();

    GError* error = nullptr;
    gchar* raw
        = secret_password_lookup_sync(schema(), nullptr, &error, kAttributeService,
                                      service.constData(), kAttributeId, key.constData(), nullptr);
    if (error)
    {
        g_error_free(error);
        return {};
    }
    if (!raw)
    {
        return {};
    }
    const QString result = QString::fromUtf8(raw);
    secret_password_free(raw);
    return result;
}

bool SecureStorageLinux::remove(const QString& id)
{
    const QByteArray service = m_service.toUtf8();
    const QByteArray key = id.toUtf8();

    GError* error = nullptr;
    const gboolean ok
        = secret_password_clear_sync(schema(), nullptr, &error, kAttributeService,
                                     service.constData(), kAttributeId, key.constData(), nullptr);
    if (error)
    {
        g_error_free(error);
        return false;
    }
    return ok;
}
