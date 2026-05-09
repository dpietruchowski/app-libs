#include "textmatcher.h"

#include <QStringList>

const QString TextMatcher::kPunctuationCharacters = ",.!?:;\"'()-¡¿";

QString TextMatcher::removePunctuation(const QString& text)
{
    QString cleaned = text;
    for (auto c : kPunctuationCharacters)
    {
        cleaned.remove(c);
    }
    return cleaned;
}

bool TextMatcher::compare(const QString& a, const QString& b)
{
    return a.compare(b, Qt::CaseInsensitive) == 0;
}

bool TextMatcher::existsInSentence(const QString& text, const QString& sentenceText)
{
    QStringList words = sentenceText.split(' ', Qt::SkipEmptyParts);
    QString cleanTarget = removePunctuation(text).trimmed();

    for (const QString& word : words)
    {
        QString cleanWord = removePunctuation(word).trimmed();
        if (compare(cleanWord, cleanTarget))
        {
            return true;
        }
    }
    return false;
}
