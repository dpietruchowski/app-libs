#include "syllabifier.h"

#include <algorithm>

namespace
{

bool isHangulSyllable(QChar letter)
{
    return letter.unicode() >= 0xAC00 && letter.unicode() <= 0xD7A3;
}

QChar baseLetter(QChar letter)
{
    const QString decomposition = letter.decomposition();
    return decomposition.isEmpty() ? letter : decomposition.front();
}

QStringList longestFirst(QStringList groups)
{
    std::stable_sort(groups.begin(), groups.end(),
                     [](const QString& a, const QString& b) { return a.size() > b.size(); });
    return groups;
}

}

Syllabifier::Syllabifier(const QString& languageCode)
    : m_language(languageCode.left(2).toLower())
{
    if (m_language == "pl")
    {
        m_consonantGroups = { "dź", "dż", "ch", "cz", "dz", "rz", "sz" };
        m_yIsVowel = true;
        m_softeningI = true;
    }
    else if (m_language == "en")
    {
        m_consonantGroups = { "ch", "sh", "th", "ph", "wh", "gh", "qu", "ck" };
        m_codaOnlyGroups = { "ck" };
        m_vowelGroups = { "eau", "ai", "ay", "au", "aw", "ea", "ee", "ei", "ey", "eu", "ew",
                          "ie",  "oa", "oe", "oi", "oo", "ou", "ow", "oy", "ue", "ui", "uy" };
        m_silentFinalE = true;
    }
    else if (m_language == "de")
    {
        m_consonantGroups = { "sch", "ch", "ck", "ph", "th", "qu" };
        m_vowelGroups = { "aa", "ee", "oo", "ie", "ei", "ai", "ey", "ay", "au", "eu", "äu" };
        m_yIsVowel = true;
    }
    else if (m_language == "fr")
    {
        m_consonantGroups = { "ch", "gn", "ph", "qu", "th" };
        m_vowelGroups = { "eau", "au", "ai", "ei", "oi", "ou", "eu", "œu", "ui", "ie" };
        m_yIsVowel = true;
        m_silentFinalE = true;
    }
    else if (m_language == "es")
    {
        m_consonantGroups = { "ch", "ll", "rr", "qu" };
        m_vowelGroups = { "ai", "au", "ei", "eu", "oi", "ou", "ia", "ie", "io",
                          "iu", "ua", "ue", "uo", "ui", "ay", "ey", "oy", "uy" };
    }
    else if (m_language == "it")
    {
        m_consonantGroups = { "ch", "gh", "gn", "gl", "qu" };
        m_vowelGroups = { "ia", "ie", "io", "iu", "ua", "ue", "uo", "ui", "ai", "ei", "oi", "au" };
    }
    else if (m_language == "pt")
    {
        m_consonantGroups = { "ch", "lh", "nh", "qu" };
        m_vowelGroups = { "ai", "au", "ei", "eu", "oi", "ou", "ui", "iu", "ão", "ãe", "õe" };
    }
    else if (m_language == "ro")
    {
        m_consonantGroups = { "ch", "gh" };
        m_vowelGroups = { "ea", "oa", "ia", "ie", "io", "iu", "ua", "uă",
                          "ai", "ei", "oi", "ui", "au", "eu", "ou" };
    }

    m_vowelGroups = longestFirst(m_vowelGroups);
    m_consonantGroups = longestFirst(m_consonantGroups);
}

QStringList Syllabifier::split(const QString& word) const
{
    const QString text = word.toLower();
    if (text.isEmpty())
        return {};

    if (std::any_of(text.begin(), text.end(), isHangulSyllable))
    {
        QStringList letters;
        for (const QChar& letter : text)
            letters.append(QString(letter));
        return letters;
    }

    QList<Unit> parts = units(text);
    silenceFinalE(parts);

    QList<int> nuclei;
    for (int i = 0; i < parts.size(); ++i)
    {
        if (parts[i].vowel)
            nuclei.append(i);
    }
    if (nuclei.isEmpty())
        return chunks(text);

    QList<int> boundaries;
    for (int i = 1; i < nuclei.size(); ++i)
        boundaries.append(nuclei[i] - onsetLength(parts, nuclei[i - 1] + 1, nuclei[i]));

    QStringList syllables;
    QString current;
    for (int i = 0; i < parts.size(); ++i)
    {
        if (boundaries.contains(i) && !current.isEmpty())
        {
            syllables.append(current);
            current.clear();
        }
        current += parts[i].text;
    }
    syllables.append(current);
    return syllables;
}

QList<Syllabifier::Unit> Syllabifier::units(const QString& word) const
{
    QList<Unit> result;
    int position = 0;
    while (position < word.size())
    {
        const QChar letter = word[position];
        const bool modifier = letter.isMark() || letter == u'ь' || letter == u'ъ';
        const bool softening = m_softeningI && letter == u'i' && position + 1 < word.size()
                               && isVowelLetter(word[position + 1]);
        if (!result.isEmpty() && !result.last().vowel && (modifier || softening))
        {
            result.last().text += letter;
            ++position;
            continue;
        }

        if (const QString group = matchGroup(word, position, m_vowelGroups); !group.isEmpty())
        {
            result.append({ group, true, false });
            position += group.size();
            continue;
        }

        if (const QString group = matchGroup(word, position, m_consonantGroups); !group.isEmpty())
        {
            result.append({ group, false, m_codaOnlyGroups.contains(group) });
            position += group.size();
            continue;
        }

        result.append({ QString(letter), isVowelAt(word, position), false });
        ++position;
    }
    return result;
}

void Syllabifier::silenceFinalE(QList<Unit>& units) const
{
    if (!m_silentFinalE)
        return;

    const auto vowels = std::count_if(units.begin(), units.end(),
                                      [](const Unit& unit) { return unit.vowel; });
    if (vowels < 2)
        return;

    int e = units.size() - 1;
    QString ending;
    if (e >= 0 && !units[e].vowel)
    {
        ending = units[e].text;
        --e;
    }

    const QStringList endings = m_language == "en" ? QStringList{ "", "s", "d" }
                                                   : QStringList{ "", "s" };
    if (e < 2 || units[e].text != "e" || !endings.contains(ending))
        return;

    if (!keepsFinalE(units, e, ending))
        units[e].vowel = false;
}

bool Syllabifier::keepsFinalE(const QList<Unit>& units, int e, const QString& ending) const
{
    const Unit& before = units[e - 1];
    const bool syllabicLiquid = !units[e - 2].vowel
                                && (before.text == "l" || (m_language == "fr" && before.text == "r"));

    if (m_language == "fr")
        return syllabicLiquid;

    if (before.vowel)
        return true;
    if (ending == "d")
        return before.text == "t" || before.text == "d";
    if (ending == "s")
    {
        static const QStringList sibilants = { "s", "x", "z", "ch", "sh", "c", "g" };
        return syllabicLiquid || sibilants.contains(before.text);
    }
    return syllabicLiquid;
}

bool Syllabifier::isVowelLetter(QChar letter) const
{
    if (letter == u'y')
        return m_yIsVowel;
    if (letter == u'й')
        return false;
    if (letter == u'ı' || letter == u'œ' || letter == u'æ')
        return true;

    static const QString vowels = QStringLiteral("aeiouаеиоуыэюяіє");
    return vowels.contains(baseLetter(letter));
}

bool Syllabifier::isVowelAt(const QString& word, int position) const
{
    const QChar letter = word[position];
    if (letter != u'y' || m_language != "en")
        return isVowelLetter(letter);

    const bool beforeVowel = position + 1 < word.size() && isVowelLetter(word[position + 1]);
    return position > 0 && !beforeVowel;
}

QString Syllabifier::matchGroup(const QString& word, int position, const QStringList& groups)
{
    for (const QString& group : groups)
    {
        if (QStringView(word).mid(position).startsWith(group))
            return group;
    }
    return {};
}

int Syllabifier::onsetLength(const QList<Unit>& units, int from, int to)
{
    const int size = to - from;
    if (size == 0 || units[to - 1].codaOnly)
        return 0;
    if (size >= 2 && isObstruent(units[to - 2]) && isLiquid(units[to - 1]))
        return 2;
    return 1;
}

bool Syllabifier::isObstruent(const Unit& unit)
{
    static const QStringList obstruents = { "p", "b", "t", "d", "k", "g", "f", "v", "c", "ch", "ph",
                                            "th", "п", "б", "т", "д", "к", "г", "ф", "в", "х" };
    return obstruents.contains(unit.text);
}

bool Syllabifier::isLiquid(const Unit& unit)
{
    static const QStringList liquids = { "r", "l", "rz", "р", "л" };
    return liquids.contains(unit.text);
}

QStringList Syllabifier::chunks(const QString& word)
{
    QStringList result;
    for (int i = 0; i < word.size(); i += fallbackChunkSize)
        result.append(word.mid(i, fallbackChunkSize));

    if (result.size() > 1 && result.last().size() == 1)
        result[result.size() - 2] += result.takeLast();
    return result;
}
