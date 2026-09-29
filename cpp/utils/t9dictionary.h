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

    static QString digitsFor(const QString& word);

    void setWords(const QStringList& words);
    void setUsage(const QString& word, const Usage& usage);
    Usage learn(const QString& word, qint64 usedAt);

    Usage usage(const QString& word) const;
    bool contains(const QString& word) const;
    int size() const;
    QStringList candidates(const QString& digits, int limit) const;

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

    std::string_view digitsOf(const Entry& entry) const;
    std::vector<Entry>::const_iterator find(const std::string& letters,
                                            const std::string& digits) const;
    Entry append(const std::string& letters, const std::string& digits);
    Usage usageOf(const Entry& entry) const;

    std::string m_letters;
    std::string m_digits;
    std::vector<Entry> m_entries;
    QHash<quint32, Usage> m_usage;
};
