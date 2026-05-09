#include "textmatcher.h"

#include <QDebug>
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
    QStringList textWords = text.split(' ', Qt::SkipEmptyParts);

    auto it = std::search(words.begin(), words.end(), textWords.begin(), textWords.end(),
                          [](const QString& a, const QString& b)
                          {
                              QString cleanA = removePunctuation(a).trimmed();
                              QString cleanB = removePunctuation(b).trimmed();

                              qDebug() << "Comparing:" << cleanA << "vs" << cleanB;

                              return compare(cleanA, cleanB);
                          });

    return it != words.end();
}
