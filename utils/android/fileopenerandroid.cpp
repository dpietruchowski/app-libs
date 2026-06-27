#include "fileopenerandroid.h"

#include "jni/intent.h"

#include <QJniEnvironment>
#include <QJniObject>

namespace
{
constexpr int kOpenDocumentRequestCode = 0x4F50454E;
constexpr int kResultOk = -1;

bool readFromUri(const QJniObject& uri, QByteArray& data, QString& error)
{
    QJniObject context(QNativeInterface::QAndroidApplication::context());

    QJniObject resolver
        = context.callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");

    if (!resolver.isValid())
    {
        error = QStringLiteral("No content resolver available");
        return false;
    }

    QJniObject stream = resolver.callObjectMethod(
        "openInputStream", "(Landroid/net/Uri;)Ljava/io/InputStream;", uri.object<jobject>());

    QJniEnvironment env;

    if (env.checkAndClearExceptions() || !stream.isValid())
    {
        error = QStringLiteral("Could not open selected file");
        return false;
    }

    QByteArray result;

    constexpr jint kBufferSize = 8192;

    jbyteArray buffer = env->NewByteArray(kBufferSize);

    if (!buffer)
    {
        stream.callMethod<void>("close", "()V");
        error = QStringLiteral("Out of memory");
        return false;
    }

    while (true)
    {
        jint bytesRead = stream.callMethod<jint>("read", "([B)I", buffer);

        if (env.checkAndClearExceptions())
        {
            env->DeleteLocalRef(buffer);
            stream.callMethod<void>("close", "()V");

            error = QStringLiteral("Failed while reading file");
            return false;
        }

        if (bytesRead <= 0)
        {
            break;
        }

        jbyte* raw = env->GetByteArrayElements(buffer, nullptr);

        result.append(reinterpret_cast<char*>(raw), bytesRead);

        env->ReleaseByteArrayElements(buffer, raw, JNI_ABORT);
    }

    stream.callMethod<void>("close", "()V");
    env.checkAndClearExceptions();

    env->DeleteLocalRef(buffer);

    data = std::move(result);
    return true;
}
}  // namespace

FileOpenerAndroid::FileOpenerAndroid(OpenedCallback onOpened, CancelledCallback onCancelled,
                                     FailedCallback onFailed)
    : m_onOpened(std::move(onOpened))
    , m_onCancelled(std::move(onCancelled))
    , m_onFailed(std::move(onFailed))
{
}

void FileOpenerAndroid::launch(const QString& mimeType)
{
    using android::Intent;

    Intent intent(Intent::Action::OpenDocument);

    intent.addCategory(Intent::Category::Openable);

    intent.setType(mimeType.isEmpty() ? QStringLiteral("*/*") : mimeType);

    QtAndroidPrivate::startActivity(intent.jniObject(), kOpenDocumentRequestCode, this);
}

void FileOpenerAndroid::handleActivityResult(int receiverRequestCode, int resultCode,
                                             const QJniObject& data)
{
    if (receiverRequestCode != kOpenDocumentRequestCode)
    {
        return;
    }

    if (resultCode != kResultOk)
    {
        if (m_onCancelled)
        {
            m_onCancelled();
        }
        return;
    }

    const QJniObject uri
        = data.isValid() ? data.callObjectMethod("getData", "()Landroid/net/Uri;") : QJniObject();

    if (!uri.isValid())
    {
        if (m_onFailed)
        {
            m_onFailed(QStringLiteral("No file selected"));
        }
        return;
    }

    QByteArray content;
    QString error;

    if (readFromUri(uri, content, error))
    {
        if (m_onOpened)
        {
            m_onOpened(uri.toString(), content);
        }
    }
    else if (m_onFailed)
    {
        m_onFailed(error);
    }
}