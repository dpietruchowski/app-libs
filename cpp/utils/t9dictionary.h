#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <string>
#include <string_view>
#include <vector>

class T9Dictionary final
{
public:
    struct Usage
    {
        int count = 0;
        qint64 lastUsed = 0;

        bool operator==(const Usage&) const = default;
    };

    static QString normalized(const QString& word);

    void setWords(const QStringList& words);
    void setUsage(const QString& word, const Usage& usage);
    Usage learn(const QString& word, qint64 usedAt);

    Usage usage(const QString& word) const;
    bool contains(const QString& word) const;
    int size() const;
    QStringList candidatesForGroups(const QStringList& groups, int limit) const;

private:
    struct Entry
    {
        quint32 offset = 0;
        quint32 length = 0;
    };

    struct Ranked
    {
        Entry entry;
        Usage usage;
    };

    using EntryIterator = std::vector<Entry>::const_iterator;

    std::u16string_view wordOf(const Entry& entry) const;
    EntryIterator find(std::u16string_view word) const;
    Entry append(std::u16string_view word);
    Usage usageOf(const Entry& entry) const;
    void collectMatches(EntryIterator first, EntryIterator last, std::size_t depth,
                        const std::vector<std::u16string>& groups, std::vector<Ranked>& exact,
                        std::vector<Ranked>& longer) const;
    QStringList rankedWords(std::vector<Ranked>& exact, std::vector<Ranked>& longer,
                            int limit) const;

    std::u16string m_letters;
    std::vector<Entry> m_entries;
    QHash<quint32, Usage> m_usage;
};
