#include "urlhelper.h"

#include <QDesktopServices>
#include <QUrl>
#include <QUrlQuery>

#ifdef Q_OS_ANDROID
#include "intent.h"
#endif

UrlHelper::UrlHelper(QObject* parent)
    : QObject(parent)
{
}

QString UrlHelper::googleTranslateUrl(const QString& text, const QString& sourceLanguage,
                                      const QString& targetLanguage)
{
    QUrl url("https://translate.google.com/");

    QUrlQuery query;
    query.addQueryItem("sl", languageToCode(sourceLanguage));
    query.addQueryItem("tl", languageToCode(targetLanguage));
    query.addQueryItem("text", text);
    query.addQueryItem("op", "translate");

    url.setQuery(query);

    return url.toString();
}

void UrlHelper::openTranslate(const QString& text, const QString& sourceLanguage,
                              const QString& targetLanguage)
{
#ifdef Q_OS_ANDROID
    android::Intent sendIntent(android::Intent::Action::Send);
    sendIntent.setType(android::Intent::MimeType::TextPlain)
        .setClassName("com.google.android.apps.translate",
                      "com.google.android.apps.translate.TranslateActivity")
        .putExtra(android::Intent::Extra::Text, text)
        .putExtra("key_text_input", text)
        .putExtra("key_language_from", languageToCode(sourceLanguage))
        .putExtra("key_language_to", languageToCode(targetLanguage));

    if (sendIntent.resolves())
    {
        sendIntent.start();
        return;
    }

    android::Intent processTextIntent(android::Intent::Action::ProcessText);
    processTextIntent.setType(android::Intent::MimeType::TextPlain)
        .setPackage("com.google.android.apps.translate")
        .putExtra(android::Intent::Extra::ProcessText, text)
        .putExtra(android::Intent::Extra::ProcessTextReadonly, true);

    if (processTextIntent.resolves())
    {
        processTextIntent.start();
        return;
    }
#endif
    QDesktopServices::openUrl(QUrl(googleTranslateUrl(text, sourceLanguage, targetLanguage)));
}

QString UrlHelper::languageToCode(const QString& language)
{
    QString lower = language.toLower();
    if (lower == "english" || lower == "en")
        return "en";
    if (lower == "polish" || lower == "pl")
        return "pl";
    if (lower == "german" || lower == "de")
        return "de";
    if (lower == "french" || lower == "fr")
        return "fr";
    if (lower == "spanish" || lower == "es")
        return "es";
    if (lower == "japanese" || lower == "ja")
        return "ja";
    if (lower == "chinese" || lower == "zh")
        return "zh";
    if (lower == "italian" || lower == "it")
        return "it";
    if (lower == "korean" || lower == "ko")
        return "ko";
    if (lower == "russian" || lower == "ru")
        return "ru";
    if (lower == "portuguese" || lower == "pt")
        return "pt";
    if (lower == "arabic" || lower == "ar")
        return "ar";

    return lower;
}
