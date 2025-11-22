#include "fillintextbuilder.h"

FillInTextBuilder::FillInTextBuilder(const QString& targetWord, const QString& sentence)
    : m_targetWord(targetWord)
    , m_sentence(sentence)
{
}

QString FillInTextBuilder::buildPartialText(const QString& chosenWord) const
{
    if (chosenWord.isEmpty())
        return replaceInText(m_targetWord, "_____");
    QString correctWord = "<span class=\"green\">" + m_targetWord + "</span>";
    if (compareWords(m_targetWord, chosenWord))
        return replaceInText(m_targetWord, correctWord);
    return replaceInText(m_targetWord,
                         "<span class=\"wrong\">" + chosenWord + "</span> " + correctWord);
}

QString FillInTextBuilder::buildHighlightedText() const
{
    QStringList words = m_sentence.split(' ');
    for (auto& word : words)
    {
        QString cleanWord = word;
        removePunctuation(cleanWord);

        if (compareWords(cleanWord, m_targetWord))
        {
            word = QString("<span class=\"highlighted\">%1</span>").arg(word);
        }
    }
    return words.join(' ');
}

QString FillInTextBuilder::replaceInText(const QString& originalWord,
                                         const QString& replacement) const
{
    QStringList words = m_sentence.split(' ');
    for (auto& word : words)
    {
        QString cleanWord = word;
        removePunctuation(cleanWord);
        if (compareWords(cleanWord, originalWord))
            word = replacement;
    }
    return words.join(' ');
}

bool FillInTextBuilder::compareWords(const QString& a, const QString& b) const
{
    return a.compare(b, Qt::CaseInsensitive) == 0;
}

void FillInTextBuilder::removePunctuation(QString& word) const
{
    QString characters = ",.!?:;";
    for (auto c : characters)
        word.remove(c);
}
