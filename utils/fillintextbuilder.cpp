#include "fillintextbuilder.h"
#include "wordmatcher.h"

FillInTextBuilder::FillInTextBuilder(const QStringList& targetWords, const QString& sentence)
    : m_targetWords(targetWords)
    , m_sentence(sentence)
{
}

void FillInTextBuilder::setAttempt(const QString& word, const QString& answer)
{
    m_attempts[word] = answer;
}

void FillInTextBuilder::setAttempt(int index, const QString& answer)
{
    if (index >= 0 && index < m_targetWords.size())
    {
        m_attempts[m_targetWords[index]] = answer;
    }
}

QString FillInTextBuilder::buildPartialText() const
{
    return transformWords(
        [this](const QString& word, int targetIndex)
        {
            if (targetIndex == -1)
                return word;

            QString targetWord = m_targetWords[targetIndex];
            QString attempt = m_attempts.value(targetWord, "");

            if (attempt.isEmpty())
            {
                return QString("_____");
            }

            if (WordMatcher::compareWords(targetWord, attempt))
            {
                return wrapInSpan(word, SpanClass::Correct);
            }

            QString wrongSpan = wrapInSpan(attempt, SpanClass::Wrong);
            QString correctSpan = wrapInSpan(word, SpanClass::Correct);
            return wrongSpan + " " + correctSpan;
        });
}

QString FillInTextBuilder::buildPreviewText(const QString& preview) const
{
    return transformWords(
        [this, &preview](const QString& word, int targetIndex)
        {
            if (targetIndex == -1)
                return word;

            if (preview.isEmpty())
            {
                return QString("_____");
            }

            return wrapInSpan(preview, SpanClass::Highlighted);
        });
}

QString FillInTextBuilder::buildHighlightedText() const
{
    return transformWords(
        [this](const QString& word, int targetIndex)
        { return targetIndex != -1 ? wrapInSpan(word, SpanClass::Highlighted) : word; });
}

QString
FillInTextBuilder::transformWords(std::function<QString(const QString&, int)> callback) const
{
    QStringList words = m_sentence.split(' ');
    for (auto& word : words)
    {
        QString cleanWord = WordMatcher::removePunctuation(word);
        int targetIndex = -1;

        for (int i = 0; i < m_targetWords.size(); ++i)
        {
            if (WordMatcher::compareWords(cleanWord, m_targetWords[i]))
            {
                targetIndex = i;
                break;
            }
        }

        word = callback(word, targetIndex);
    }
    return words.join(' ');
}

QString FillInTextBuilder::wrapInSpan(const QString& text, SpanClass spanClass) const
{
    if (spanClass == SpanClass::None)
        return text;

    WordParts parts = splitWordParts(text);

    QString className;
    switch (spanClass)
    {
        case SpanClass::Correct:
            className = "correct";
            break;
        case SpanClass::Wrong:
            className = "wrong";
            break;
        case SpanClass::Highlighted:
            className = "highlighted";
            break;
        default:
            return text;
    }

    QString span = QString("<span class=\"%1\">%2</span>").arg(className, parts.core);
    return parts.prefix + span + parts.suffix;
}

FillInTextBuilder::WordParts FillInTextBuilder::splitWordParts(const QString& word) const
{
    int start = 0;
    int end = word.length();

    while (start < end && WordMatcher::kPunctuationCharacters.contains(word[start]))
        start++;

    while (end > start && WordMatcher::kPunctuationCharacters.contains(word[end - 1]))
        end--;

    WordParts parts;
    parts.prefix = word.left(start);
    parts.core = word.mid(start, end - start);
    parts.suffix = word.mid(end);

    return parts;
}
