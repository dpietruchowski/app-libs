#pragma once
#include <QMap>
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
    explicit FillInTextBuilder(const QStringList& targetWords, const QString& sentence);

    void setAttempt(const QString& word, const QString& answer);
    void setAttempt(int index, const QString& answer);

    QString buildPartialText() const;
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
    transformWords(std::function<QString(const QString& word, int targetIndex)> callback) const;
    QString wrapInSpan(const QString& text, SpanClass spanClass) const;
    QString findOriginalWord(const QString& targetWord) const;

    QStringList m_targetWords;
    QString m_sentence;
    QMap<QString, QString> m_attempts;
};
