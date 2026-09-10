#include "texttospeechandroid.h"

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QDebug>
#include <QJniEnvironment>
#include <QJniObject>

TextToSpeechAndroid::TextToSpeechAndroid(QObject* parent)
    : QObject(parent)
{
    initialize();
}

TextToSpeechAndroid::~TextToSpeechAndroid()
{
    if (m_tts.isValid())
    {
        m_tts.callMethod<void>("shutdown");
    }
}

void TextToSpeechAndroid::initialize()
{
    auto context = QNativeInterface::QAndroidApplication::context();
    if (!context.isValid())
    {
        qWarning() << "Failed to get Android context for TTS";
        return;
    }

    m_tts
        = QJniObject("android/speech/tts/TextToSpeech",
                     "(Landroid/content/Context;Landroid/speech/tts/TextToSpeech$OnInitListener;)V",
                     context.object(), nullptr);

    if (!m_tts.isValid())
    {
        qWarning() << "Failed to create Android TextToSpeech object";
        return;
    }

    m_initialized = true;
}

void TextToSpeechAndroid::speak(const QString& text, const QString& languageCode)
{
    if (!m_initialized || !m_tts.isValid())
    {
        qWarning() << "TextToSpeech not initialized on Android";
        return;
    }

    // Set language
    QStringList parts = languageCode.split('_');
    QString language = parts.isEmpty() ? "en" : parts[0];
    QString country = parts.size() > 1 ? parts[1] : "";

    QJniObject jLanguage = QJniObject::fromString(language);
    QJniObject jCountry = QJniObject::fromString(country);
    QJniObject locale("java/util/Locale", "(Ljava/lang/String;Ljava/lang/String;)V",
                      jLanguage.object<jstring>(), jCountry.object<jstring>());

    m_tts.callMethod<jint>("setLanguage", "(Ljava/util/Locale;)I", locale.object());

    // Speak
    QJniObject jText = QJniObject::fromString(text);
    QJniObject jUtteranceId = QJniObject::fromString("utteranceId");

    m_tts.callMethod<jint>("speak",
                           "(Ljava/lang/CharSequence;ILandroid/os/Bundle;Ljava/lang/String;)I",
                           jText.object<jstring>(),
                           0,  // QUEUE_FLUSH
                           nullptr, jUtteranceId.object<jstring>());
}

bool TextToSpeechAndroid::isInitialized() const { return m_initialized; }

#endif  // Q_OS_ANDROID
