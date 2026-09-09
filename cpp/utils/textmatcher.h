#pragma once

#include <QString>

struct TextSpan final
{
    qsizetype position { -1 };
    qsizetype length { 0 };

    bool isValid() const { return position >= 0; }

    bool operator==(const TextSpan& other) const = default;
};

class TextMatcher final
{
public:
    static const QString kPunctuationCharacters;

    static QString removePunctuation(const QString& text);
    static QString foldAccents(const QString& text);
    static bool compare(const QString& a, const QString& b, bool ignoreAccents = false);
    static bool answersMatch(const QString& userAnswer, const QString& correctAnswer,
                             bool ignoreAccents = false);
    static TextSpan spanInSentence(const QString& text, const QString& sentenceText);
    static bool existsInSentence(const QString& text, const QString& sentenceText);
    static int editDistance(const QString& a, const QString& b);
};
