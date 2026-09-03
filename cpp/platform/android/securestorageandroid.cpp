#include "securestorageandroid.h"

#include <QByteArray>
#include <QJniEnvironment>
#include <QJniObject>
#include <QSettings>

namespace
{
const char* kAndroidKeyStore = "AndroidKeyStore";
const char* kTransformation = "AES/GCM/NoPadding";
const char* kSettingsApplication = "securestorage";
const int kGcmTagLengthBits = 128;

jbyteArray toJByteArray(QJniEnvironment& env, const QByteArray& data)
{
    jbyteArray array = env->NewByteArray(data.size());
    env->SetByteArrayRegion(array, 0, data.size(),
                            reinterpret_cast<const jbyte*>(data.constData()));
    return array;
}

QByteArray fromJByteArray(QJniEnvironment& env, jbyteArray array)
{
    if (!array)
    {
        return {};
    }
    const jsize length = env->GetArrayLength(array);
    QByteArray out(length, Qt::Uninitialized);
    env->GetByteArrayRegion(array, 0, length, reinterpret_cast<jbyte*>(out.data()));
    return out;
}

jobjectArray singleStringArray(QJniEnvironment& env, const QString& value)
{
    // QJniEnvironment::findClass returns a cached global reference owned by Qt;
    // it must not be released with DeleteLocalRef.
    jclass stringClass = env.findClass("java/lang/String");
    return env->NewObjectArray(1, stringClass, QJniObject::fromString(value).object());
}

QJniObject openKeyStore()
{
    QJniEnvironment env;
    QJniObject keyStore = QJniObject::callStaticObjectMethod(
        "java/security/KeyStore", "getInstance", "(Ljava/lang/String;)Ljava/security/KeyStore;",
        QJniObject::fromString(QLatin1String(kAndroidKeyStore)).object<jstring>());
    if (!keyStore.isValid())
    {
        return {};
    }
    keyStore.callMethod<void>("load", "(Ljava/security/KeyStore$LoadStoreParameter;)V",
                              static_cast<jobject>(nullptr));
    if (env.checkAndClearExceptions())
    {
        return {};
    }
    return keyStore;
}

bool aliasExists(QJniObject& keyStore, const QString& alias)
{
    QJniEnvironment env;
    const jboolean has = keyStore.callMethod<jboolean>(
        "containsAlias", "(Ljava/lang/String;)Z", QJniObject::fromString(alias).object<jstring>());
    if (env.checkAndClearExceptions())
    {
        return false;
    }
    return has;
}

bool generateKey(const QString& alias)
{
    QJniEnvironment env;
    const jint purposeEncrypt = QJniObject::getStaticField<jint>(
        "android/security/keystore/KeyProperties", "PURPOSE_ENCRYPT");
    const jint purposeDecrypt = QJniObject::getStaticField<jint>(
        "android/security/keystore/KeyProperties", "PURPOSE_DECRYPT");

    QJniObject builder("android/security/keystore/KeyGenParameterSpec$Builder",
                       "(Ljava/lang/String;I)V", QJniObject::fromString(alias).object<jstring>(),
                       purposeEncrypt | purposeDecrypt);
    if (!builder.isValid())
    {
        return false;
    }

    jobjectArray blockModes = singleStringArray(env, QStringLiteral("GCM"));
    builder = builder.callObjectMethod(
        "setBlockModes",
        "([Ljava/lang/String;)Landroid/security/keystore/KeyGenParameterSpec$Builder;", blockModes);

    jobjectArray paddings = singleStringArray(env, QStringLiteral("NoPadding"));
    builder = builder.callObjectMethod(
        "setEncryptionPaddings",
        "([Ljava/lang/String;)Landroid/security/keystore/KeyGenParameterSpec$Builder;", paddings);

    builder = builder.callObjectMethod(
        "setKeySize", "(I)Landroid/security/keystore/KeyGenParameterSpec$Builder;", jint { 256 });

    QJniObject spec
        = builder.callObjectMethod("build", "()Landroid/security/keystore/KeyGenParameterSpec;");
    if (!spec.isValid())
    {
        return false;
    }

    QJniObject keyGenerator = QJniObject::callStaticObjectMethod(
        "javax/crypto/KeyGenerator", "getInstance",
        "(Ljava/lang/String;Ljava/lang/String;)Ljavax/crypto/KeyGenerator;",
        QJniObject::fromString(QStringLiteral("AES")).object<jstring>(),
        QJniObject::fromString(QLatin1String(kAndroidKeyStore)).object<jstring>());
    if (!keyGenerator.isValid())
    {
        return false;
    }

    keyGenerator.callMethod<void>("init", "(Ljava/security/spec/AlgorithmParameterSpec;)V",
                                  spec.object());
    if (env.checkAndClearExceptions())
    {
        return false;
    }
    keyGenerator.callObjectMethod("generateKey", "()Ljavax/crypto/SecretKey;");
    if (env.checkAndClearExceptions())
    {
        return false;
    }
    return true;
}

QJniObject secretKey(const QString& alias, bool create)
{
    QJniObject keyStore = openKeyStore();
    if (!keyStore.isValid())
    {
        return {};
    }
    if (!aliasExists(keyStore, alias))
    {
        if (!create || !generateKey(alias))
        {
            return {};
        }
        keyStore = openKeyStore();
        if (!keyStore.isValid())
        {
            return {};
        }
    }

    QJniEnvironment env;
    QJniObject key = keyStore.callObjectMethod(
        "getKey", "(Ljava/lang/String;[C)Ljava/security/Key;",
        QJniObject::fromString(alias).object<jstring>(), static_cast<jcharArray>(nullptr));
    if (env.checkAndClearExceptions())
    {
        return {};
    }
    return key;
}
}

SecureStorageAndroid::SecureStorageAndroid(const QString& organization, const QString& serviceName)
    : m_organization(organization)
    , m_serviceName(serviceName)
{
}

QSettings SecureStorageAndroid::settings() const
{
    return QSettings(m_organization, QLatin1String(kSettingsApplication));
}

QString SecureStorageAndroid::settingsKey(const QString& id) const
{
    return m_serviceName + QLatin1Char('/') + id;
}

bool SecureStorageAndroid::isAvailable() const { return openKeyStore().isValid(); }

bool SecureStorageAndroid::contains(const QString& id) const
{
    return settings().contains(settingsKey(id));
}

bool SecureStorageAndroid::store(const QString& id, const QString& secret)
{
    QJniObject key = secretKey(m_serviceName, /*create=*/true);
    if (!key.isValid())
    {
        return false;
    }

    QJniEnvironment env;
    QJniObject cipher = QJniObject::callStaticObjectMethod(
        "javax/crypto/Cipher", "getInstance", "(Ljava/lang/String;)Ljavax/crypto/Cipher;",
        QJniObject::fromString(QLatin1String(kTransformation)).object<jstring>());
    if (!cipher.isValid())
    {
        return false;
    }

    const jint encryptMode
        = QJniObject::getStaticField<jint>("javax/crypto/Cipher", "ENCRYPT_MODE");
    cipher.callMethod<void>("init", "(ILjava/security/Key;)V", encryptMode, key.object());
    if (env.checkAndClearExceptions())
    {
        return false;
    }

    QJniObject ivObject = cipher.callObjectMethod("getIV", "()[B");
    const QByteArray iv = fromJByteArray(env, ivObject.object<jbyteArray>());

    jbyteArray plainArray = toJByteArray(env, secret.toUtf8());
    QJniObject cipherObject = cipher.callObjectMethod("doFinal", "([B)[B", plainArray);
    env->DeleteLocalRef(plainArray);
    if (env.checkAndClearExceptions() || !cipherObject.isValid())
    {
        return false;
    }
    const QByteArray cipherText = fromJByteArray(env, cipherObject.object<jbyteArray>());

    const QString blob = QString::fromLatin1(iv.toBase64()) + QLatin1Char(':')
        + QString::fromLatin1(cipherText.toBase64());
    settings().setValue(settingsKey(id), blob);
    return true;
}

QString SecureStorageAndroid::retrieve(const QString& id) const
{
    const QString blob = settings().value(settingsKey(id)).toString();
    if (blob.isEmpty())
    {
        return {};
    }
    const QStringList parts = blob.split(QLatin1Char(':'));
    if (parts.size() != 2)
    {
        return {};
    }
    const QByteArray iv = QByteArray::fromBase64(parts.at(0).toLatin1());
    const QByteArray cipherText = QByteArray::fromBase64(parts.at(1).toLatin1());

    QJniObject key = secretKey(m_serviceName, /*create=*/false);
    if (!key.isValid())
    {
        return {};
    }

    QJniEnvironment env;
    QJniObject cipher = QJniObject::callStaticObjectMethod(
        "javax/crypto/Cipher", "getInstance", "(Ljava/lang/String;)Ljavax/crypto/Cipher;",
        QJniObject::fromString(QLatin1String(kTransformation)).object<jstring>());
    if (!cipher.isValid())
    {
        return {};
    }

    jbyteArray ivArray = toJByteArray(env, iv);
    QJniObject gcmSpec("javax/crypto/spec/GCMParameterSpec", "(I[B)V", jint { kGcmTagLengthBits },
                       ivArray);
    env->DeleteLocalRef(ivArray);
    if (!gcmSpec.isValid())
    {
        return {};
    }

    const jint decryptMode
        = QJniObject::getStaticField<jint>("javax/crypto/Cipher", "DECRYPT_MODE");
    cipher.callMethod<void>("init",
                            "(ILjava/security/Key;Ljava/security/spec/AlgorithmParameterSpec;)V",
                            decryptMode, key.object(), gcmSpec.object());
    if (env.checkAndClearExceptions())
    {
        return {};
    }

    jbyteArray cipherArray = toJByteArray(env, cipherText);
    QJniObject plainObject = cipher.callObjectMethod("doFinal", "([B)[B", cipherArray);
    env->DeleteLocalRef(cipherArray);
    if (env.checkAndClearExceptions() || !plainObject.isValid())
    {
        return {};
    }
    return QString::fromUtf8(fromJByteArray(env, plainObject.object<jbyteArray>()));
}

bool SecureStorageAndroid::remove(const QString& id)
{
    settings().remove(settingsKey(id));
    return true;
}
