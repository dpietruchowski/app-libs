#include "textmatcher.h"

#include <QDebug>
#include <QStringList>

const QString TextMatcher::kPunctuationCharacters = ",.!?:;\"()¡¿";

QString TextMatcher::removePunctuation(const QString& text)
{
    QString cleaned = text;
    for (auto c : kPunctuationCharacters)
    {
        cleaned.remove(c);
    }
    return cleaned;
}

QString TextMatcher::foldAccents(const QString& text)
{
    QString decomposed = text.normalized(QString::NormalizationForm_D);
    QString result;
    result.reserve(decomposed.size());
    for (QChar c : decomposed)
    {
        QChar::Category category = c.category();
        if (category != QChar::Mark_NonSpacing && category != QChar::Mark_SpacingCombining
            && category != QChar::Mark_Enclosing)
        {
            result.append(c);
        }
    }
    return result;
}

bool TextMatcher::compare(const QString& a, const QString& b, bool ignoreAccents)
{
    if (ignoreAccents)
        return foldAccents(a).compare(foldAccents(b), Qt::CaseInsensitive) == 0;
    return a.compare(b, Qt::CaseInsensitive) == 0;
}

bool TextMatcher::answersMatch(const QString& userAnswer, const QString& correctAnswer,
                               bool ignoreAccents)
{
    return compare(removePunctuation(userAnswer).trimmed(),
                   removePunctuation(correctAnswer).trimmed(), ignoreAccents);
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

                              return compare(cleanA, cleanB);
                          });

    return it != words.end();
}
