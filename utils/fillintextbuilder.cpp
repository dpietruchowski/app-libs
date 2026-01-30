#include "fillintextbuilder.h"
#include "wordmatcher.h"

FillInTextBuilder::FillInTextBuilder(const QString& targetWord, const QString& sentence)
    : m_targetWord(targetWord)
    , m_sentence(sentence)
{
}

QString FillInTextBuilder::buildPartialText(const QString& chosenWord) const
{
    if (chosenWord.isEmpty())
    {
        return transformWords(m_targetWord, [](const QString& word, bool isTarget)
                              { return isTarget ? "_____" : word; });
    }

    if (WordMatcher::compareWords(m_targetWord, chosenWord))
    {
        return transformWords(m_targetWord, [this](const QString& word, bool isTarget)
                              { return isTarget ? wrapInSpan(word, SpanClass::Correct) : word; });
    }

    return transformWords(m_targetWord,
                          [this, &chosenWord](const QString& word, bool isTarget)
                          {
                              if (!isTarget)
                                  return word;
                              QString wrongSpan = wrapInSpan(chosenWord, SpanClass::Wrong);
                              QString correctSpan = wrapInSpan(word, SpanClass::Correct);
                              return wrongSpan + " " + correctSpan;
                          });
}

QString FillInTextBuilder::buildHighlightedText() const
{
    return transformWords(m_targetWord, [this](const QString& word, bool isTarget)
                          { return isTarget ? wrapInSpan(word, SpanClass::Highlighted) : word; });
}

QString
FillInTextBuilder::transformWords(const QString& targetWord,
                                  std::function<QString(const QString&, bool)> callback) const
{
    QStringList words = m_sentence.split(' ');
    for (auto& word : words)
    {
        QString cleanWord = WordMatcher::removePunctuation(word);
        bool isTarget = WordMatcher::compareWords(cleanWord, targetWord);
        word = callback(word, isTarget);
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
