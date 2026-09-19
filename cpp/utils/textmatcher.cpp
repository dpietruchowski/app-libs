#include "textmatcher.h"

#include <QDebug>
#include <QStringList>

#include <algorithm>
#include <vector>

const QString TextMatcher::kPunctuationCharacters = QStringLiteral(",.!?:;\"()¡¿،؟؛„“”«»–—…");

QString TextMatcher::removePunctuation(const QString& text)
{
    QString cleaned = text;
    for (auto c : kPunctuationCharacters)
    {
        cleaned.remove(c);
    }
    return cleaned;
}

QString TextMatcher::foldAccents(const QString& text)
{
    QString decomposed = text.normalized(QString::NormalizationForm_D);
    QString result;
    result.reserve(decomposed.size());
    for (QChar c : decomposed)
    {
        QChar::Category category = c.category();
        if (category != QChar::Mark_NonSpacing && category != QChar::Mark_SpacingCombining
            && category != QChar::Mark_Enclosing)
        {
            result.append(c);
        }
    }
    return result;
}

bool TextMatcher::compare(const QString& a, const QString& b, bool ignoreAccents)
{
    if (ignoreAccents)
        return foldAccents(a).compare(foldAccents(b), Qt::CaseInsensitive) == 0;
    return a.compare(b, Qt::CaseInsensitive) == 0;
}

bool TextMatcher::answersMatch(const QString& userAnswer, const QString& correctAnswer,
                               bool ignoreAccents)
{
    return compare(removePunctuation(userAnswer).trimmed(),
                   removePunctuation(correctAnswer).trimmed(), ignoreAccents);
}

namespace
{
enum class Gap
{
    Joined,
    Hyphen,
    Separated
};

struct Piece
{
    qsizetype start = 0;
    qsizetype length = 0;
    Gap gap = Gap::Separated;
};

enum class DigitGroups
{
    Split,
    Joined
};

bool isDigitSeparator(QStringView text, qsizetype i)
{
    return (text[i] == u',' || text[i] == u'.') && i > 0 && i + 1 < text.size()
        && text[i - 1].isDigit() && text[i + 1].isDigit();
}

bool isBoundary(QStringView text, qsizetype i, DigitGroups digitGroups)
{
    if (digitGroups == DigitGroups::Joined && isDigitSeparator(text, i))
        return false;
    const QChar c = text[i];
    return c.isSpace() || c == u'-' || TextMatcher::kPunctuationCharacters.contains(c);
}

QString withoutDigitSeparators(QStringView text)
{
    QString result;
    result.reserve(text.size());
    for (qsizetype i = 0; i < text.size(); ++i)
    {
        if (!isDigitSeparator(text, i))
            result.append(text[i]);
    }
    return result;
}

bool isApostrophe(QChar c) { return c == u'\'' || c == u'’'; }

bool isAffixingScript(QChar c)
{
    const char16_t code = c.unicode();
    return (code >= 0xAC00 && code <= 0xD7A3) || (code >= 0x0621 && code <= 0x064A);
}

bool isAllAffixingScript(QStringView text)
{
    if (text.isEmpty())
        return false;
    for (QChar c : text)
    {
        if (!isAffixingScript(c))
            return false;
    }
    return true;
}

Gap gapBetween(QStringView between)
{
    if (between.isEmpty())
        return Gap::Joined;
    if (between == u"-")
        return Gap::Hyphen;
    return Gap::Separated;
}

QList<Piece> splitPieces(const QString& text, DigitGroups digitGroups)
{
    QList<Piece> pieces;
    qsizetype previousEnd = -1;
    qsizetype i = 0;
    while (i < text.size())
    {
        if (isBoundary(text, i, digitGroups))
        {
            ++i;
            continue;
        }

        const qsizetype start = i;
        while (i < text.size() && !isBoundary(text, i, digitGroups))
        {
            const bool elision = isApostrophe(text[i]) && i > start && i + 1 < text.size()
                && !isBoundary(text, i + 1, digitGroups) && !isApostrophe(text[i + 1]);
            ++i;
            if (elision)
                break;
        }

        const Gap gap = previousEnd < 0
            ? Gap::Separated
            : gapBetween(QStringView(text).mid(previousEnd, start - previousEnd));
        pieces.append(Piece { start, i - start, gap });
        previousEnd = i;
    }
    return pieces;
}

std::optional<TextSpan> matchPiece(QStringView word, QStringView target, bool allowPrefix,
                                   bool allowSuffix, DigitGroups digitGroups)
{
    if (word.compare(target, Qt::CaseInsensitive) == 0)
        return TextSpan { 0, word.size() };
    if (digitGroups == DigitGroups::Joined
        && withoutDigitSeparators(word).compare(withoutDigitSeparators(target), Qt::CaseInsensitive)
            == 0)
        return TextSpan { 0, word.size() };
    if (target.isEmpty() || word.size() <= target.size())
        return std::nullopt;

    const qsizetype rest = word.size() - target.size();
    if (allowPrefix && isAffixingScript(target.back())
        && word.startsWith(target, Qt::CaseInsensitive) && isAllAffixingScript(word.last(rest)))
        return TextSpan { 0, target.size() };
    if (allowSuffix && isAffixingScript(target.front())
        && word.endsWith(target, Qt::CaseInsensitive) && isAllAffixingScript(word.first(rest)))
        return TextSpan { rest, target.size() };
    return std::nullopt;
}

std::optional<TextSpan> findPieces(const QString& text, const QString& sentenceText, qsizetype from,
                                   DigitGroups digitGroups)
{
    const QList<Piece> targets = splitPieces(text, digitGroups);
    const QList<Piece> words = splitPieces(sentenceText, digitGroups);
    if (targets.isEmpty())
        return std::nullopt;

    const qsizetype last = targets.size() - 1;
    for (qsizetype first = 0; first + targets.size() <= words.size(); ++first)
    {
        if (words[first].start + words[first].length <= from)
            continue;

        qsizetype start = 0;
        qsizetype end = 0;
        bool matches = true;
        for (qsizetype k = 0; k <= last && matches; ++k)
        {
            const Piece& word = words[first + k];
            const Piece& target = targets[k];
            if (k > 0 && word.gap != target.gap)
            {
                matches = false;
                break;
            }

            const QStringView wordView = QStringView(sentenceText).sliced(word.start, word.length);
            const QStringView targetView = QStringView(text).sliced(target.start, target.length);
            auto span = matchPiece(wordView, targetView, k == last, k == 0, digitGroups);
            if (span && k == 0 && word.start + span->start < from)
                span = matchPiece(wordView, targetView, false, true, digitGroups);
            if (!span || (k == 0 && word.start + span->start < from)
                || (k < last && span->start + span->length != word.length)
                || (k > 0 && span->start != 0))
            {
                matches = false;
                break;
            }
            if (k == 0)
                start = word.start + span->start;
            end = word.start + span->start + span->length;
        }

        if (matches)
            return TextSpan { start, end - start };
    }
    return std::nullopt;
}
}

bool TextMatcher::existsInSentence(const QString& text, const QString& sentenceText)
{
    return findInSentence(text, sentenceText).has_value();
}

int TextMatcher::editDistance(const QString& a, const QString& b)
{
    if (a.isEmpty())
    {
        return static_cast<int>(b.size());
    }
    if (b.isEmpty())
    {
        return static_cast<int>(a.size());
    }

    std::vector<int> previous(static_cast<size_t>(b.size()) + 1);
    std::vector<int> current(static_cast<size_t>(b.size()) + 1);
    for (size_t column = 0; column < previous.size(); ++column)
    {
        previous[column] = static_cast<int>(column);
    }

    for (qsizetype row = 1; row <= a.size(); ++row)
    {
        current[0] = static_cast<int>(row);
        for (qsizetype column = 1; column <= b.size(); ++column)
        {
            const size_t index = static_cast<size_t>(column);
            const int substitution
                = previous[index - 1] + (a.at(row - 1) == b.at(column - 1) ? 0 : 1);
            current[index]
                = std::min({ substitution, previous[index] + 1, current[index - 1] + 1 });
        }
        previous.swap(current);
    }
    return previous[static_cast<size_t>(b.size())];
}

std::optional<TextSpan> TextMatcher::findInSentence(const QString& text,
                                                    const QString& sentenceText, qsizetype from)
{
    if (const auto span = findPieces(text, sentenceText, from, DigitGroups::Split))
        return span;
    return findPieces(text, sentenceText, from, DigitGroups::Joined);
}
