#pragma once

#include <QList>
#include <QString>
#include <QStringList>

class Syllabifier final
{
public:
    explicit Syllabifier(const QString& languageCode);

    QStringList split(const QString& word) const;

private:
    struct Unit
    {
        QString text;
        bool vowel = false;
        bool codaOnly = false;
    };

    static constexpr int fallbackChunkSize = 3;

    QList<Unit> units(const QString& word) const;
    void silenceFinalE(QList<Unit>& units) const;
    bool keepsFinalE(const QList<Unit>& units, int e, const QString& ending) const;
    bool isVowelLetter(QChar letter) const;
    bool isVowelAt(const QString& word, int position) const;

    static QString matchGroup(const QString& word, int position, const QStringList& groups);
    static int onsetLength(const QList<Unit>& units, int from, int to);
    static bool isObstruent(const Unit& unit);
    static bool isLiquid(const Unit& unit);
    static QStringList chunks(const QString& word);

    QString m_language;
    QStringList m_vowelGroups;
    QStringList m_consonantGroups;
    QStringList m_codaOnlyGroups;
    bool m_yIsVowel = false;
    bool m_softeningI = false;
    bool m_silentFinalE = false;
};
