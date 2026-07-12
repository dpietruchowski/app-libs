#pragma once

#include <QString>

class TextMatcher final
{
public:
    static const QString kPunctuationCharacters;

    static QString removePunctuation(const QString& text);
    static QString foldAccents(const QString& text);
    static bool compare(const QString& a, const QString& b, bool ignoreAccents = false);
    static bool existsInSentence(const QString& text, const QString& sentenceText);
};
