#include "filesaverandroid.h"

#include "jni/intent.h"

#include <QJniEnvironment>
#include <QJniObject>

namespace
{
constexpr int kCreateDocumentRequestCode = 0x46494C45;
constexpr int kResultOk = -1;

bool writeToUri(const QJniObject& uri, const QByteArray& payload, QString& error)
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
        "openOutputStream", "(Landroid/net/Uri;)Ljava/io/OutputStream;", uri.object<jobject>());

    QJniEnvironment env;
    if (env.checkAndClearExceptions() || !stream.isValid())
    {
        error = QStringLiteral("Could not open the selected destination");
        return false;
    }

    const jsize length = static_cast<jsize>(payload.size());
    jbyteArray bytes = env->NewByteArray(length);
    if (bytes == nullptr)
    {
        stream.callMethod<void>("close", "()V");
        env.checkAndClearExceptions();
        error = QStringLiteral("Out of memory while writing document");
        return false;
    }
    env->SetByteArrayRegion(bytes, 0, length, reinterpret_cast<const jbyte*>(payload.constData()));

    stream.callMethod<void>("write", "([B)V", bytes);
    const bool wrote = !env.checkAndClearExceptions();
    stream.callMethod<void>("flush", "()V");
    env.checkAndClearExceptions();
    stream.callMethod<void>("close", "()V");
    env.checkAndClearExceptions();
    env->DeleteLocalRef(bytes);

    if (!wrote)
    {
        error = QStringLiteral("Failed to write the document");
        return false;
    }
    return true;
}
}  // namespace

FileSaverAndroid::FileSaverAndroid(SavedCallback onSaved, CancelledCallback onCancelled,
                                   FailedCallback onFailed)
    : m_onSaved(std::move(onSaved))
    , m_onCancelled(std::move(onCancelled))
    , m_onFailed(std::move(onFailed))
{
}

void FileSaverAndroid::launch(const QString& suggestedName, const QString& mimeType,
                              const QByteArray& data)
{
    using android::Intent;

    m_pending = data;

    Intent intent(Intent::Action::CreateDocument);
    intent.addCategory(Intent::Category::Openable);
    intent.setType(mimeType.isEmpty() ? QStringLiteral("application/octet-stream") : mimeType);
    intent.putExtra(Intent::Extra::Title, suggestedName);

    QtAndroidPrivate::startActivity(intent.jniObject(), kCreateDocumentRequestCode, this);
}

void FileSaverAndroid::handleActivityResult(int receiverRequestCode, int resultCode,
                                            const QJniObject& data)
{
    if (receiverRequestCode != kCreateDocumentRequestCode)
    {
        return;
    }

    const QByteArray payload = m_pending;
    m_pending.clear();

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
            m_onFailed(QStringLiteral("No destination was selected"));
        }
        return;
    }

    QString error;
    if (writeToUri(uri, payload, error))
    {
        if (m_onSaved)
        {
            m_onSaved(uri.toString());
        }
    }
    else if (m_onFailed)
    {
        m_onFailed(error);
    }
}
