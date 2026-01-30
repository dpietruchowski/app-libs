#include "wordmatcher.h"

#include <QStringList>

const QString WordMatcher::kPunctuationCharacters = ",.!?:;\"'()-¡¿";

QString WordMatcher::removePunctuation(const QString& word)
{
    QString cleaned = word;
    for (auto c : kPunctuationCharacters)
    {
        cleaned.remove(c);
    }
    return cleaned;
}

bool WordMatcher::compareWords(const QString& a, const QString& b)
{
    return a.compare(b, Qt::CaseInsensitive) == 0;
}

bool WordMatcher::wordExistsInSentence(const QString& wordInSentence, const QString& sentenceText)
{
    QStringList words = sentenceText.split(' ', Qt::SkipEmptyParts);
    QString cleanTarget = removePunctuation(wordInSentence).trimmed();

    for (const QString& word : words)
    {
        QString cleanWord = removePunctuation(word).trimmed();
        if (compareWords(cleanWord, cleanTarget))
        {
            return true;
        }
    }
    return false;
}
