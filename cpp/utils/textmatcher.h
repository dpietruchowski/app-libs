#pragma once

#include <QString>
#include <optional>

struct TextSpan
{
    qsizetype start = 0;
    qsizetype length = 0;
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
    static bool existsInSentence(const QString& text, const QString& sentenceText);
    static std::optional<TextSpan> findInSentence(const QString& text, const QString& sentenceText,
                                                  qsizetype from = 0);
};
