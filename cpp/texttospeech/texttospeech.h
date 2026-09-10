#pragma once

#include <QObject>
#include <QString>
#include <memory>

class TextToSpeech final : public QObject
{
    Q_OBJECT

public:
    explicit TextToSpeech(QObject* parent = nullptr);
    ~TextToSpeech() override;

    void say(const QString& text, const QString& languageCode);
    bool isAvailable() const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
