#include "bitsparser.h"

#include "diagram/parsersupport.h"

#include <QRegularExpression>

#include <algorithm>
#include <optional>

namespace diagram
{
namespace
{
    using parsing::Error;
    using parsing::fragment;
    using parsing::labelText;
    using parsing::statement;
    using Handled = std::optional<Error>;

    constexpr qsizetype kMaxSpans = 300;
    constexpr int kMaxBits = 128;
    constexpr int kMaxRowBytes = 16;
    constexpr int kMaxByteIndex = 1023;

    const QString kIndexes = QStringLiteral(R"re((\d{1,9})(?:\s*\.\.\s*(\d{1,9}))?)re");

    const QString kUnderstands = QStringLiteral(
        "A bits diagram here understands: bits N (a register of 1 to 128 bits) or bytes W (a "
        "memory layout drawn W bytes per row, 1 to 16) as the first line after the title, field "
        "0..7 label \"description\" (a single index or a range in either order, the description "
        "is optional), pad 8..15 and title");

    struct Form
    {
        QRegularExpression pattern;
        QString keyword;
        QString form;
    };

    const QList<Form>& knownForms()
    {
        static const QList<Form> forms {
            { fragment(QStringLiteral(R"(^bits\b)")), QStringLiteral("bits"),
              QStringLiteral("bits N, with N from 1 to 128") },
            { fragment(QStringLiteral(R"(^bytes\b)")), QStringLiteral("bytes"),
              QStringLiteral("bytes W, with W from 1 to 16") },
            { fragment(QStringLiteral(R"(^field\b)")), QStringLiteral("field"),
              QStringLiteral("field 0..7 label \"description\", with the description optional") },
            { fragment(QStringLiteral(R"(^pad\b)")), QStringLiteral("pad"),
              QStringLiteral("pad 8..15") },
            { fragment(QStringLiteral(R"(^title\b)")), QStringLiteral("title"),
              QStringLiteral("title Text") },
        };
        return forms;
    }

    struct Indexes
    {
        int low = 0;
        int high = 0;
        bool isValid = false;
    };

    Indexes indexesOf(const QString& first, const QString& second)
    {
        const int a = first.toInt();
        const int b = second.isEmpty() ? a : second.toInt();
        return { std::min(a, b), std::max(a, b), true };
    }

    class BitsReader
    {
    public:
        Result<BitsDiagram> read(const QString& text)
        {
            if (text.size() > parsing::kMaxLength)
                return Result<BitsDiagram>::failure(parsing::tooLongMessage());
            const QStringList lines = parsing::normalized(text).split(QLatin1Char('\n'));
            for (qsizetype index = 0; index < lines.size(); ++index)
            {
                m_lineNumber = int(index) + 1;
                const QString line = lines.at(index).trimmed();
                if (!line.isEmpty())
                    m_lastContentLine = m_lineNumber;
                if (const Error error = readLine(line))
                    return Result<BitsDiagram>::failure(*error);
            }
            if (const Error error = finish())
                return Result<BitsDiagram>::failure(*error);
            return Result<BitsDiagram>::success(m_diagram);
        }

    private:
        Error readLine(const QString& line)
        {
            if (m_skippedBlocks.isOpen())
            {
                m_skippedBlocks.continueWith(line);
                return std::nullopt;
            }
            if (line.isEmpty() || line.startsWith(QLatin1Char('\'')))
                return std::nullopt;
            if (const auto warning = m_skippedBlocks.start(line, m_lineNumber))
            {
                if (!warning->isEmpty())
                    m_diagram.warnings.append(parsing::lineErrorText(m_lineNumber, *warning));
                return std::nullopt;
            }
            return readStatement(line);
        }

        Error readStatement(const QString& line)
        {
            static const QRegularExpression title = statement(QStringLiteral(R"(title\s+(.+))"));

            if (parsing::isDiagramMarker(line) || parsing::isBitsMarker(line))
                return std::nullopt;
            if (const auto match = title.match(line); match.hasMatch())
            {
                m_diagram.title = labelText(match.captured(1));
                return std::nullopt;
            }
            for (const auto reader :
                 { &BitsReader::readMode, &BitsReader::readField, &BitsReader::readPadding })
            {
                if (const auto handled = (this->*reader)(line))
                    return *handled;
            }
            return lineError(diagnosis(line));
        }

        Handled readMode(const QString& line)
        {
            static const QRegularExpression mode
                = statement(QStringLiteral(R"((bits|bytes)\s+(\d{1,9}))"));
            const auto match = mode.match(line);
            if (!match.hasMatch())
                return std::nullopt;

            const bool inBits
                = match.captured(1).compare(QStringLiteral("bits"), Qt::CaseInsensitive) == 0;
            if (m_hasMode)
                return lineError(QStringLiteral("the layout is already set to \"%1\" at line %2. "
                                                "Use one \"bits N\" or \"bytes W\" line.")
                                     .arg(m_modeText)
                                     .arg(m_modeLine));
            const int width = match.captured(2).toInt();
            const int most = inBits ? kMaxBits : kMaxRowBytes;
            if (width < 1 || width > most)
                return lineError(
                    QStringLiteral("\"%1\" is out of range. Use %2 from 1 to %3.")
                        .arg(line, inBits ? QStringLiteral("bits") : QStringLiteral("bytes"))
                        .arg(most));
            m_hasMode = true;
            m_modeText = line;
            m_modeLine = m_lineNumber;
            m_diagram.mode = inBits ? BitsMode::Bits : BitsMode::Bytes;
            m_diagram.width = width;
            m_owner = QList<int>(inBits ? width : kMaxByteIndex + 1, -1);
            return Error {};
        }

        Handled readField(const QString& line)
        {
            static const QRegularExpression field = statement(
                QStringLiteral(R"re(field\s+%1\s+([^"]+?)(?:\s+"([^"]*)")?)re").arg(kIndexes));
            const auto match = field.match(line);
            if (!match.hasMatch())
                return std::nullopt;
            return append(BitsSpanKind::Field, indexesOf(match.captured(1), match.captured(2)),
                          labelText(match.captured(3)), labelText(match.captured(4)));
        }

        Handled readPadding(const QString& line)
        {
            static const QRegularExpression padding
                = statement(QStringLiteral(R"re(pad\s+%1)re").arg(kIndexes));
            const auto match = padding.match(line);
            if (!match.hasMatch())
                return std::nullopt;
            return append(BitsSpanKind::Padding, indexesOf(match.captured(1), match.captured(2)),
                          QString(), QString());
        }

        Handled append(BitsSpanKind kind, const Indexes& indexes, const QString& label,
                       const QString& description)
        {
            if (!m_hasMode)
                return lineError(QStringLiteral("the layout is missing. Start with \"bits N\" or "
                                                "\"bytes W\" before the first field or pad."));
            if (const Error error = checkRange(indexes))
                return error;
            if (const Error error = checkOverlap(indexes))
                return error;
            if (m_diagram.spans.size() >= kMaxSpans)
                return lineError(
                    QStringLiteral("the diagram has more than %1 fields and pads").arg(kMaxSpans));

            BitsSpan span;
            span.low = indexes.low;
            span.high = indexes.high;
            span.kind = kind;
            span.label = label;
            span.description = description;
            span.line = m_lineNumber;
            for (int index = span.low; index <= span.high; ++index)
                m_owner[index] = int(m_diagram.spans.size());
            m_diagram.spans.append(span);
            return Error {};
        }

        Error checkRange(const Indexes& indexes) const
        {
            if (m_diagram.mode == BitsMode::Bits)
            {
                if (indexes.high < m_diagram.width)
                    return std::nullopt;
                return lineError(QStringLiteral("%1 is outside \"%2\". Use bits 0 to %3.")
                                     .arg(describedRange(indexes), m_modeText)
                                     .arg(m_diagram.width - 1));
            }
            if (indexes.high <= kMaxByteIndex)
                return std::nullopt;
            return lineError(QStringLiteral("%1 is beyond the largest layout of %2 bytes. Use "
                                            "bytes 0 to %3.")
                                 .arg(describedRange(indexes))
                                 .arg(kMaxByteIndex + 1)
                                 .arg(kMaxByteIndex));
        }

        Error checkOverlap(const Indexes& indexes) const
        {
            for (int index = indexes.low; index <= indexes.high; ++index)
            {
                if (m_owner.at(index) < 0)
                    continue;
                const BitsSpan& other = m_diagram.spans.at(m_owner.at(index));
                const QString what = other.kind == BitsSpanKind::Padding
                    ? QStringLiteral("the padding")
                    : QStringLiteral("the field \"%1\"").arg(other.label);
                return lineError(
                    QStringLiteral("%1 overlaps %2 at line %3 (%4). Give every %5 to one field or "
                                   "pad.")
                        .arg(describedRange(indexes), what)
                        .arg(other.line)
                        .arg(describedRange({ other.low, other.high, true }), unit()));
            }
            return std::nullopt;
        }

        QString unit() const
        {
            return m_diagram.mode == BitsMode::Bits ? QStringLiteral("bit")
                                                    : QStringLiteral("byte");
        }

        QString describedRange(const Indexes& indexes) const
        {
            if (indexes.low == indexes.high)
                return QStringLiteral("%1 %2").arg(unit()).arg(indexes.low);
            return QStringLiteral("%1s %2..%3").arg(unit()).arg(indexes.low).arg(indexes.high);
        }

        QString diagnosis(const QString& line) const
        {
            for (const Form& form : knownForms())
            {
                if (form.pattern.match(line).hasMatch())
                    return QStringLiteral(
                               "\"%1\" is not a valid \"%2\" statement. Write it as: %3.")
                        .arg(line, form.keyword, form.form);
            }
            return QStringLiteral("unknown statement \"%1\". %2.").arg(line, kUnderstands);
        }

        Error finish()
        {
            if (const auto unclosed = m_skippedBlocks.unclosed())
                return lineErrorAt(
                    unclosed->first,
                    QStringLiteral("\"%1\" block is not closed").arg(unclosed->second));
            if (m_lastContentLine == 0)
                return QStringLiteral("The diagram is empty");
            if (!m_hasMode)
                return lineErrorAt(m_lastContentLine,
                                   QStringLiteral("the layout is missing. Start with \"bits N\" "
                                                  "or \"bytes W\" before the fields."));
            if (m_diagram.spans.isEmpty())
                return lineErrorAt(m_lastContentLine,
                                   QStringLiteral("the diagram has no fields. Add field lines "
                                                  "after \"%1\".")
                                       .arg(m_modeText));
            return std::nullopt;
        }

        Error lineError(const QString& text) const { return lineErrorAt(m_lineNumber, text); }

        static Error lineErrorAt(int line, const QString& text)
        {
            return parsing::lineErrorText(line, text);
        }

        BitsDiagram m_diagram;
        QList<int> m_owner;
        bool m_hasMode = false;
        QString m_modeText;
        int m_modeLine = 0;
        int m_lineNumber = 0;
        int m_lastContentLine = 0;
        parsing::SkippedBlocks m_skippedBlocks;
    };
}

Result<BitsDiagram> BitsParser::parse(const QString& text) { return BitsReader().read(text); }
}
