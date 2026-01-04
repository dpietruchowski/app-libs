#include "urlhelper.h"

#include <QUrl>
#include <QUrlQuery>

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
