#include "fillintextbuilder.h"

namespace
{
const QString kPunctuationCharacters = ",.!?:;\"'()-¡¿";
}

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

    if (compareWords(m_targetWord, chosenWord))
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
        QString cleanWord = word;
        removePunctuation(cleanWord);
        bool isTarget = compareWords(cleanWord, targetWord);
        word = callback(word, isTarget);
    }
    return words.join(' ');
}

bool FillInTextBuilder::compareWords(const QString& a, const QString& b) const
{
    return a.compare(b, Qt::CaseInsensitive) == 0;
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

    while (start < end && kPunctuationCharacters.contains(word[start]))
        start++;

    while (end > start && kPunctuationCharacters.contains(word[end - 1]))
        end--;

    WordParts parts;
    parts.prefix = word.left(start);
    parts.core = word.mid(start, end - start);
    parts.suffix = word.mid(end);

    return parts;
}

void FillInTextBuilder::removePunctuation(QString& word) const
{
    for (auto c : kPunctuationCharacters)
        word.remove(c);
}
