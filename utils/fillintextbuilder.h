#pragma once
#include <QString>
#include <QStringList>
#include <functional>

enum class SpanClass
{
    None,
    Correct,
    Wrong,
    Highlighted
};

class FillInTextBuilder
{
public:
    explicit FillInTextBuilder(const QString& targetWord, const QString& sentence);

    QString buildPartialText(const QString& chosenWord = QString()) const;
    QString buildHighlightedText() const;

private:
    struct WordParts
    {
        QString prefix;
        QString core;
        QString suffix;
    };

    WordParts splitWordParts(const QString& word) const;
    QString
    transformWords(const QString& targetWord,
                   std::function<QString(const QString& word, bool isTarget)> callback) const;
    QString wrapInSpan(const QString& text, SpanClass spanClass) const;
    bool compareWords(const QString& a, const QString& b) const;
    void removePunctuation(QString& word) const;
    QString findOriginalWord(const QString& targetWord) const;

    QString m_targetWord;
    QString m_sentence;
};
