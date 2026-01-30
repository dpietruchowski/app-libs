#pragma once

#include <QString>

class WordMatcher final
{
public:
    static const QString kPunctuationCharacters;

    static QString removePunctuation(const QString& word);
    static bool compareWords(const QString& a, const QString& b);
    static bool wordExistsInSentence(const QString& wordInSentence, const QString& sentenceText);
};
