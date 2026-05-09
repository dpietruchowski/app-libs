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
    explicit FillInTextBuilder(const QStringList& targetExpressions, const QString& sentence);

    void setAttempt(const QString& expression, const QString& answer);
    void setAttempt(int index, const QString& answer);

    QString buildPartialText() const;
    QString buildPreviewText(const QString& preview) const;
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

    QStringList m_targetExpressions;
    QString m_sentence;
    QMap<QString, QString> m_attempts;
};
