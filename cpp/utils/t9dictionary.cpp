#include "t9dictionary.h"

#include <algorithm>
#include <tuple>

namespace
{

constexpr std::string_view kDigitForLetter = "22233344455566677778889999";

std::string lettersOf(const QString& word)
{
    std::string letters;
    letters.reserve(static_cast<std::size_t>(word.size()));
    for (const QChar character : word)
    {
        const char16_t lower = character.toLower().unicode();
        if (lower < u'a' || lower > u'z')
        {
            return {};
        }
        letters.push_back(static_cast<char>(lower));
    }
    return letters;
}

std::string digitsOfLetters(const std::string& letters)
{
    std::string digits;
    digits.reserve(letters.size());
    for (const char letter : letters)
    {
        digits.push_back(kDigitForLetter[static_cast<std::size_t>(letter - 'a')]);
    }
    return digits;
}

std::string keyOf(const QString& digits)
{
    std::string key;
    key.reserve(static_cast<std::size_t>(digits.size()));
    for (const QChar character : digits)
    {
        const char16_t digit = character.unicode();
        if (digit < u'2' || digit > u'9')
        {
            return {};
        }
        key.push_back(static_cast<char>(digit));
    }
    return key;
}

}

QString T9Dictionary::digitsFor(const QString& word)
{
    return QString::fromLatin1(digitsOfLetters(lettersOf(word)));
}

void T9Dictionary::setWords(const QStringList& words)
{
    m_letters.clear();
    m_digits.clear();
    m_entries.clear();
    m_usage.clear();
    m_entries.reserve(static_cast<std::size_t>(words.size()));

    for (const QString& word : words)
    {
        const std::string letters = lettersOf(word.trimmed());
        if (letters.empty())
        {
            continue;
        }
        const Entry entry { static_cast<quint32>(m_letters.size()),
                            static_cast<quint32>(letters.size()) };
        m_letters += letters;
        m_digits += digitsOfLetters(letters);
        m_entries.push_back(entry);
    }

    const auto keyOfEntry = [this](const Entry& entry)
    {
        return std::pair(digitsOf(entry),
                         std::string_view(m_letters).substr(entry.offset, entry.length));
    };
    std::sort(m_entries.begin(), m_entries.end(),
              [&keyOfEntry](const Entry& left, const Entry& right)
              {
                  const auto leftKey = keyOfEntry(left);
                  const auto rightKey = keyOfEntry(right);
                  return leftKey != rightKey ? leftKey < rightKey : left.offset < right.offset;
              });
    m_entries.erase(std::unique(m_entries.begin(), m_entries.end(),
                                [&keyOfEntry](const Entry& left, const Entry& right)
                                { return keyOfEntry(left) == keyOfEntry(right); }),
                    m_entries.end());
    m_entries.shrink_to_fit();
}

void T9Dictionary::setUsage(const QString& word, const Usage& usage)
{
    const std::string letters = lettersOf(word.trimmed());
    if (letters.empty())
    {
        return;
    }
    const std::string digits = digitsOfLetters(letters);
    const auto found = find(letters, digits);
    const Entry entry = found != m_entries.end() ? *found : append(letters, digits);
    m_usage.insert(entry.offset, usage);
}

T9Dictionary::Usage T9Dictionary::learn(const QString& word, qint64 usedAt)
{
    const std::string letters = lettersOf(word.trimmed());
    if (letters.empty())
    {
        return {};
    }
    const std::string digits = digitsOfLetters(letters);
    const auto found = find(letters, digits);
    const Entry entry = found != m_entries.end() ? *found : append(letters, digits);
    Usage& usage = m_usage[entry.offset];
    ++usage.count;
    usage.lastUsed = usedAt;
    return usage;
}

T9Dictionary::Usage T9Dictionary::usage(const QString& word) const
{
    const std::string letters = lettersOf(word.trimmed());
    if (letters.empty())
    {
        return {};
    }
    const auto found = find(letters, digitsOfLetters(letters));
    return found != m_entries.end() ? usageOf(*found) : Usage();
}

bool T9Dictionary::contains(const QString& word) const
{
    const std::string letters = lettersOf(word.trimmed());
    return !letters.empty() && find(letters, digitsOfLetters(letters)) != m_entries.end();
}

int T9Dictionary::size() const { return static_cast<int>(m_entries.size()); }

QStringList T9Dictionary::candidates(const QString& digits, int limit) const
{
    const std::string key = keyOf(digits);
    if (key.empty() || limit <= 0)
    {
        return {};
    }

    const auto digitsBefore
        = [this](const Entry& entry, std::string_view value) { return digitsOf(entry) < value; };
    const auto first
        = std::lower_bound(m_entries.begin(), m_entries.end(), std::string_view(key), digitsBefore);
    const std::string pastPrefix = key + ':';
    const auto last
        = std::lower_bound(first, m_entries.end(), std::string_view(pastPrefix), digitsBefore);

    std::vector<Ranked> exact;
    std::vector<Ranked> longer;
    for (auto it = first; it != last; ++it)
    {
        (it->length == key.size() ? exact : longer).push_back({ *it, usageOf(*it) });
    }

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
    const auto appendWord = [this, &words](const Ranked& ranked) {
        words.append(
            QString::fromLatin1(m_letters.data() + ranked.entry.offset, ranked.entry.length));
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

std::string_view T9Dictionary::digitsOf(const Entry& entry) const
{
    return std::string_view(m_digits).substr(entry.offset, entry.length);
}

std::vector<T9Dictionary::Entry>::const_iterator T9Dictionary::find(const std::string& letters,
                                                                    const std::string& digits) const
{
    const std::string_view key(digits);
    const auto first = std::lower_bound(m_entries.begin(), m_entries.end(), key,
                                        [this](const Entry& entry, std::string_view value)
                                        { return digitsOf(entry) < value; });
    const auto last = std::upper_bound(first, m_entries.end(), key,
                                       [this](std::string_view value, const Entry& entry)
                                       { return value < digitsOf(entry); });
    const auto found = std::find_if(
        first, last, [this, &letters](const Entry& entry)
        { return std::string_view(m_letters).substr(entry.offset, entry.length) == letters; });
    return found != last ? found : m_entries.end();
}

T9Dictionary::Entry T9Dictionary::append(const std::string& letters, const std::string& digits)
{
    const Entry entry { static_cast<quint32>(m_letters.size()),
                        static_cast<quint32>(letters.size()) };
    m_letters += letters;
    m_digits += digits;
    const auto position = std::upper_bound(m_entries.begin(), m_entries.end(), entry,
                                           [this](const Entry& left, const Entry& right)
                                           { return digitsOf(left) < digitsOf(right); });
    m_entries.insert(position, entry);
    return entry;
}

T9Dictionary::Usage T9Dictionary::usageOf(const Entry& entry) const
{
    return m_usage.value(entry.offset);
}
