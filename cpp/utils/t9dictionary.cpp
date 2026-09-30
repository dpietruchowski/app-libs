#include "t9dictionary.h"

#include <algorithm>
#include <tuple>

namespace
{

constexpr char16_t kApostrophe = u'\'';
constexpr char16_t kTypographicApostrophe = u'’';
constexpr char16_t kHyphen = u'-';

bool isJoiner(char16_t character) { return character == kApostrophe || character == kHyphen; }

char16_t folded(QChar character)
{
    const char16_t lower = character.toLower().unicode();
    return lower == kTypographicApostrophe ? kApostrophe : lower;
}

std::u16string lettersOf(const QString& word)
{
    std::u16string letters;
    letters.reserve(static_cast<std::size_t>(word.size()));
    for (const QChar character : word.trimmed())
    {
        const char16_t letter = folded(character);
        if (isJoiner(letter))
        {
            if (letters.empty() || isJoiner(letters.back()))
            {
                return {};
            }
        }
        else if (!QChar(letter).isLetter())
        {
            return {};
        }
        letters.push_back(letter);
    }
    if (!letters.empty() && isJoiner(letters.back()))
    {
        return {};
    }
    return letters;
}

std::u16string groupOf(const QString& group)
{
    std::u16string letters;
    for (const QChar character : group)
    {
        const char16_t letter = folded(character);
        if (isJoiner(letter) || QChar(letter).isLetter())
        {
            letters.push_back(letter);
        }
    }
    return letters;
}

}

QString T9Dictionary::normalized(const QString& word)
{
    const std::u16string letters = lettersOf(word);
    return QString(reinterpret_cast<const QChar*>(letters.data()),
                   static_cast<qsizetype>(letters.size()));
}

void T9Dictionary::setWords(const QStringList& words)
{
    m_letters.clear();
    m_entries.clear();
    m_usage.clear();
    m_entries.reserve(static_cast<std::size_t>(words.size()));

    for (const QString& word : words)
    {
        const std::u16string letters = lettersOf(word);
        if (letters.empty())
        {
            continue;
        }
        m_entries.push_back(
            { static_cast<quint32>(m_letters.size()), static_cast<quint32>(letters.size()) });
        m_letters += letters;
    }

    std::sort(m_entries.begin(), m_entries.end(),
              [this](const Entry& left, const Entry& right)
              {
                  const auto leftWord = wordOf(left);
                  const auto rightWord = wordOf(right);
                  return leftWord != rightWord ? leftWord < rightWord : left.offset < right.offset;
              });
    m_entries.erase(std::unique(m_entries.begin(), m_entries.end(),
                                [this](const Entry& left, const Entry& right)
                                { return wordOf(left) == wordOf(right); }),
                    m_entries.end());
    m_entries.shrink_to_fit();
}

void T9Dictionary::setUsage(const QString& word, const Usage& usage)
{
    const std::u16string letters = lettersOf(word);
    if (letters.empty())
    {
        return;
    }
    const auto found = find(letters);
    const Entry entry = found != m_entries.end() ? *found : append(letters);
    m_usage.insert(entry.offset, usage);
}

T9Dictionary::Usage T9Dictionary::learn(const QString& word, qint64 usedAt)
{
    const std::u16string letters = lettersOf(word);
    if (letters.empty())
    {
        return {};
    }
    const auto found = find(letters);
    const Entry entry = found != m_entries.end() ? *found : append(letters);
    Usage& usage = m_usage[entry.offset];
    ++usage.count;
    usage.lastUsed = usedAt;
    return usage;
}

T9Dictionary::Usage T9Dictionary::usage(const QString& word) const
{
    const std::u16string letters = lettersOf(word);
    if (letters.empty())
    {
        return {};
    }
    const auto found = find(letters);
    return found != m_entries.end() ? usageOf(*found) : Usage();
}

bool T9Dictionary::contains(const QString& word) const
{
    const std::u16string letters = lettersOf(word);
    return !letters.empty() && find(letters) != m_entries.end();
}

int T9Dictionary::size() const { return static_cast<int>(m_entries.size()); }

QStringList T9Dictionary::candidatesForGroups(const QStringList& groups, int limit) const
{
    if (groups.isEmpty() || limit <= 0)
    {
        return {};
    }
    std::vector<std::u16string> letterGroups;
    letterGroups.reserve(static_cast<std::size_t>(groups.size()));
    for (const QString& group : groups)
    {
        std::u16string letters = groupOf(group);
        if (letters.empty())
        {
            return {};
        }
        letterGroups.push_back(std::move(letters));
    }

    std::vector<Ranked> exact;
    std::vector<Ranked> longer;
    collectMatches(m_entries.begin(), m_entries.end(), 0, letterGroups, exact, longer);
    return rankedWords(exact, longer, limit);
}

void T9Dictionary::collectMatches(EntryIterator first, EntryIterator last, std::size_t depth,
                                  const std::vector<std::u16string>& groups,
                                  std::vector<Ranked>& exact, std::vector<Ranked>& longer) const
{
    if (depth == groups.size())
    {
        for (auto it = first; it != last; ++it)
        {
            (it->length == depth ? exact : longer).push_back({ *it, usageOf(*it) });
        }
        return;
    }
    for (const char16_t letter : groups[depth])
    {
        const auto from = std::lower_bound(first, last, letter,
                                           [this, depth](const Entry& entry, char16_t value)
                                           { return entry.length <= depth || wordOf(entry)[depth] < value; });
        const auto to = std::upper_bound(from, last, letter,
                                         [this, depth](char16_t value, const Entry& entry)
                                         { return value < wordOf(entry)[depth]; });
        if (from != to)
        {
            collectMatches(from, to, depth + 1, groups, exact, longer);
        }
    }
}

QStringList T9Dictionary::rankedWords(std::vector<Ranked>& exact, std::vector<Ranked>& longer,
                                      int limit) const
{
    const auto rank = [](const Ranked& ranked)
    { return std::tuple(-ranked.usage.count, -ranked.usage.lastUsed, ranked.entry.offset); };
    const auto rankedBefore
        = [&rank](const Ranked& left, const Ranked& right) { return rank(left) < rank(right); };
    std::sort(exact.begin(), exact.end(), rankedBefore);

    const std::size_t wanted = static_cast<std::size_t>(limit);
    const std::size_t completions
        = std::min(longer.size(), wanted - std::min(wanted, exact.size()));
    std::partial_sort(longer.begin(), longer.begin() + static_cast<std::ptrdiff_t>(completions),
                      longer.end(), rankedBefore);

    QStringList words;
    words.reserve(static_cast<qsizetype>(std::min(wanted, exact.size()) + completions));
    const auto appendWord = [this, &words](const Ranked& ranked)
    {
        words.append(QString(reinterpret_cast<const QChar*>(m_letters.data() + ranked.entry.offset),
                             static_cast<qsizetype>(ranked.entry.length)));
    };
    for (std::size_t i = 0; i < exact.size() && i < wanted; ++i)
    {
        appendWord(exact[i]);
    }
    for (std::size_t i = 0; i < completions; ++i)
    {
        appendWord(longer[i]);
    }
    return words;
}

std::u16string_view T9Dictionary::wordOf(const Entry& entry) const
{
    return std::u16string_view(m_letters).substr(entry.offset, entry.length);
}

T9Dictionary::EntryIterator T9Dictionary::find(std::u16string_view word) const
{
    const auto found = std::lower_bound(m_entries.begin(), m_entries.end(), word,
                                        [this](const Entry& entry, std::u16string_view value)
                                        { return wordOf(entry) < value; });
    return found != m_entries.end() && wordOf(*found) == word ? found : m_entries.end();
}

T9Dictionary::Entry T9Dictionary::append(std::u16string_view word)
{
    const Entry entry { static_cast<quint32>(m_letters.size()),
                        static_cast<quint32>(word.size()) };
    m_letters += word;
    m_entries.insert(std::upper_bound(m_entries.begin(), m_entries.end(), entry,
                                      [this](const Entry& left, const Entry& right)
                                      { return wordOf(left) < wordOf(right); }),
                     entry);
    return entry;
}

T9Dictionary::Usage T9Dictionary::usageOf(const Entry& entry) const
{
    return m_usage.value(entry.offset);
}
