#pragma once
#include <QString>
#include <QStringList>

class FillInTextBuilder
{
public:
    explicit FillInTextBuilder(const QString& targetWord, const QString& sentence);

    QString buildPartialText(const QString& chosenWord = QString()) const;
    QString buildHighlightedText() const;

private:
    QString replaceInText(const QString& originalWord, const QString& replacement) const;
    bool compareWords(const QString& a, const QString& b) const;
    void removePunctuation(QString& word) const;

    QString m_targetWord;
    QString m_sentence;
};
