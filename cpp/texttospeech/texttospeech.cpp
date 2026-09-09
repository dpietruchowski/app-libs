#include "texttospeech.h"

#ifdef Q_OS_ANDROID
#include "texttospeechandroid.h"
#else
#include <QLocale>
#include <QTextToSpeech>
#endif

#include <QDebug>

class TextToSpeech::Impl
{
public:
#ifdef Q_OS_ANDROID
    TextToSpeechAndroid* tts = nullptr;
#else
    QTextToSpeech* tts = nullptr;
#endif
};

TextToSpeech::TextToSpeech(QObject* parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
{
#ifdef Q_OS_ANDROID
    m_impl->tts = new TextToSpeechAndroid(this);
#else
    m_impl->tts = new QTextToSpeech(this);
#endif
}

TextToSpeech::~TextToSpeech() { }

void TextToSpeech::say(const QString& text, const QString& languageCode)
{
    if (!m_impl->tts)
    {
        qWarning() << "TextToSpeech not initialized";
        return;
    }

#ifdef Q_OS_ANDROID
    m_impl->tts->speak(text, languageCode);
#else
    if (m_impl->tts->state() == QTextToSpeech::Error)
    {
        qWarning() << "QTextToSpeech is in Error state - TTS not available on this platform";
        return;
    }

    QLocale locale(languageCode);
    if (m_impl->tts->locale() != locale)
        m_impl->tts->setLocale(locale);
    m_impl->tts->say(text);
#endif
}

bool TextToSpeech::isAvailable() const
{
#ifdef Q_OS_ANDROID
    return m_impl->tts != nullptr;
#else
    return m_impl->tts && m_impl->tts->state() != QTextToSpeech::Error;
#endif
}
