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
    android::Intent intent(android::Intent::Action::ProcessText);
    intent.setType(android::Intent::MimeType::TextPlain)
        .setPackage("com.google.android.apps.translate")
        .putExtra(android::Intent::Extra::ProcessText, text)
        .putExtra(android::Intent::Extra::ProcessTextReadonly, true);

    if (intent.resolves())
    {
        intent.start();
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

    return lower;
}
