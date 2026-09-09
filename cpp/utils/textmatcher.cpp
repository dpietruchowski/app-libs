#include "textmatcher.h"

#include <QDebug>
#include <QStringList>

#include <algorithm>
#include <vector>

namespace
{

struct SentenceWord final
{
    QString cleaned;
    qsizetype position { 0 };
    qsizetype length { 0 };
};

std::vector<SentenceWord> wordsOf(const QString& text)
{
    std::vector<SentenceWord> words;
    qsizetype index = 0;
    while (index < text.size())
    {
        while (index < text.size() && text.at(index).isSpace())
        {
            ++index;
        }
        const qsizetype start = index;
        while (index < text.size() && !text.at(index).isSpace())
        {
            ++index;
        }
        if (index > start)
        {
            words.push_back(SentenceWord { TextMatcher::removePunctuation(
                                               text.mid(start, index - start)),
                                           start, index - start });
        }
    }
    return words;
}

}

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

TextSpan TextMatcher::spanInSentence(const QString& text, const QString& sentenceText)
{
    const std::vector<SentenceWord> sentenceWords = wordsOf(sentenceText);
    const std::vector<SentenceWord> searchedWords = wordsOf(text);
    if (searchedWords.empty() || searchedWords.size() > sentenceWords.size())
    {
        return {};
    }

    const auto match
        = std::search(sentenceWords.begin(), sentenceWords.end(), searchedWords.begin(),
                      searchedWords.end(), [](const SentenceWord& a, const SentenceWord& b)
                      { return compare(a.cleaned.trimmed(), b.cleaned.trimmed()); });
    if (match == sentenceWords.end())
    {
        return {};
    }

    const SentenceWord& last = *(match + static_cast<qsizetype>(searchedWords.size()) - 1);
    qsizetype start = match->position;
    qsizetype end = last.position + last.length;
    while (start < end && kPunctuationCharacters.contains(sentenceText.at(start)))
    {
        ++start;
    }
    while (end > start && kPunctuationCharacters.contains(sentenceText.at(end - 1)))
    {
        --end;
    }
    return TextSpan { start, end - start };
}

bool TextMatcher::existsInSentence(const QString& text, const QString& sentenceText)
{
    return spanInSentence(text, sentenceText).isValid();
}

int TextMatcher::editDistance(const QString& a, const QString& b)
{
    if (a.isEmpty())
    {
        return static_cast<int>(b.size());
    }
    if (b.isEmpty())
    {
        return static_cast<int>(a.size());
    }

    std::vector<int> previous(static_cast<size_t>(b.size()) + 1);
    std::vector<int> current(static_cast<size_t>(b.size()) + 1);
    for (size_t column = 0; column < previous.size(); ++column)
    {
        previous[column] = static_cast<int>(column);
    }

    for (qsizetype row = 1; row <= a.size(); ++row)
    {
        current[0] = static_cast<int>(row);
        for (qsizetype column = 1; column <= b.size(); ++column)
        {
            const size_t index = static_cast<size_t>(column);
            const int substitution
                = previous[index - 1] + (a.at(row - 1) == b.at(column - 1) ? 0 : 1);
            current[index] = std::min({ substitution, previous[index] + 1, current[index - 1] + 1 });
        }
        previous.swap(current);
    }
    return previous[static_cast<size_t>(b.size())];
}
