#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class UrlHelper final : public QObject
{
    Q_OBJECT

public:
    explicit UrlHelper(QObject* parent = nullptr);

    Q_INVOKABLE QString googleTranslateUrl(const QString& text,
                                           const QString& sourceLanguage = "en",
                                           const QString& targetLanguage = "pl");

private:
    static QString languageToCode(const QString& language);
};
