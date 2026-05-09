#include "fillintextbuilder.h"
#include "textmatcher.h"

FillInTextBuilder::FillInTextBuilder(const QStringList& targetExpressions, const QString& sentence)
    : m_targetExpressions(targetExpressions)
    , m_sentence(sentence)
{
}

void FillInTextBuilder::setAttempt(const QString& expression, const QString& answer)
{
    m_attempts[expression] = answer;
}

void FillInTextBuilder::setAttempt(int index, const QString& answer)
{
    if (index >= 0 && index < m_targetExpressions.size())
    {
        m_attempts[m_targetExpressions[index]] = answer;
    }
}

QString FillInTextBuilder::buildPartialText() const
{
    return transformWords(
        [this](const QString& word, int targetIndex)
        {
            if (targetIndex == -1)
                return word;

            QString targetExpression = m_targetExpressions[targetIndex];
            QString attempt = m_attempts.value(targetExpression, "");

            if (attempt.isEmpty())
            {
                return QString("_____");
            }

            if (TextMatcher::compare(targetExpression, attempt))
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
    QStringList result;

    int i = 0;
    while (i < words.size())
    {
        int matchedTarget = -1;
        int matchedLen = 0;

        for (int t = 0; t < m_targetExpressions.size(); ++t)
        {
            QStringList targetWords = m_targetExpressions[t].split(' ', Qt::SkipEmptyParts);
            if (targetWords.isEmpty())
                continue;
            if (i + targetWords.size() > words.size())
                continue;

            bool match = true;
            for (int k = 0; k < targetWords.size(); ++k)
            {
                QString cleanSentenceWord = TextMatcher::removePunctuation(words[i + k]);
                QString cleanTargetWord = TextMatcher::removePunctuation(targetWords[k]);
                if (!TextMatcher::compare(cleanSentenceWord, cleanTargetWord))
                {
                    match = false;
                    break;
                }
            }

            if (match && targetWords.size() > matchedLen)
            {
                matchedTarget = t;
                matchedLen = targetWords.size();
            }
        }

        if (matchedTarget != -1)
        {
            QStringList slotWords;
            for (int k = 0; k < matchedLen; ++k)
            {
                slotWords.append(words[i + k]);
            }
            QString slotText = slotWords.join(' ');
            result.append(callback(slotText, matchedTarget));
            i += matchedLen;
        }
        else
        {
            result.append(callback(words[i], -1));
            i++;
        }
    }

    return result.join(' ');
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

    while (start < end && TextMatcher::kPunctuationCharacters.contains(word[start]))
        start++;

    while (end > start && TextMatcher::kPunctuationCharacters.contains(word[end - 1]))
        end--;

    WordParts parts;
    parts.prefix = word.left(start);
    parts.core = word.mid(start, end - start);
    parts.suffix = word.mid(end);

    return parts;
}
