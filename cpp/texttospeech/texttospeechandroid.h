#pragma once

#include <QJniObject>
#include <QObject>
#include <QString>

class TextToSpeechAndroid final : public QObject
{
    Q_OBJECT

public:
    explicit TextToSpeechAndroid(QObject* parent = nullptr);
    ~TextToSpeechAndroid() override;

    void speak(const QString& text, const QString& languageCode);
    bool isInitialized() const;

private:
    void initialize();

    QJniObject m_tts;
    bool m_initialized = false;
};
