#include "sequenceparser.h"

#include <QRegularExpression>

#include <optional>

namespace diagram
{
namespace
{
    using Error = std::optional<QString>;
    using Handled = std::optional<Error>;

    const QString kName = QStringLiteral(R"((?:"[^"]+"|[\w.]+))");
    const QString kArrow = QStringLiteral(R"((<<?-(?:\[[^\]]*\])?-?|-(?:\[[^\]]*\])?-?>>?))");
    const QString kGroupKeywords = QStringLiteral("alt|opt|loop|par|break|critical|group");
    constexpr qsizetype kMaxLength = 20000;
    constexpr qsizetype kMaxSteps = 300;

    QRegularExpression::PatternOptions patternOptions()
    {
        return QRegularExpression::CaseInsensitiveOption
            | QRegularExpression::UseUnicodePropertiesOption;
    }

    QRegularExpression statement(const QString& source)
    {
        return QRegularExpression(QStringLiteral("^") + source + QStringLiteral("$"),
                                  patternOptions());
    }

    QRegularExpression fragment(const QString& source)
    {
        return QRegularExpression(source, patternOptions());
    }

    QString unquoted(const QString& token)
    {
        if (token.size() >= 2 && token.startsWith(QLatin1Char('"'))
            && token.endsWith(QLatin1Char('"')))
            return token.mid(1, token.size() - 2);
        return token;
    }

    bool isQuoted(const QString& token) { return token.startsWith(QLatin1Char('"')); }

    QString labelText(const QString& raw)
    {
        QString text = raw.trimmed();
        text.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
        return text;
    }

    QString normalized(QString text)
    {
        text.remove(QChar(0xFEFF));
        for (const QChar quote : { QChar(0x201C), QChar(0x201D), QChar(0x201E) })
            text.replace(quote, QLatin1Char('"'));
        return text;
    }

    ParticipantKind participantKind(const QString& keyword)
    {
        return keyword.compare(QStringLiteral("actor"), Qt::CaseInsensitive) == 0
            ? ParticipantKind::Actor
            : ParticipantKind::Box;
    }

    NotePlacement notePlacement(const QString& keyword)
    {
        if (keyword.compare(QStringLiteral("left"), Qt::CaseInsensitive) == 0)
            return NotePlacement::Left;
        if (keyword.compare(QStringLiteral("right"), Qt::CaseInsensitive) == 0)
            return NotePlacement::Right;
        return NotePlacement::Over;
    }

    struct SkippedBlock
    {
        QString name;
        QRegularExpression start;
        QRegularExpression end;
        bool countsBraces = false;
        bool warns = true;
    };

    const QList<SkippedBlock>& skippedBlocks()
    {
        static const QList<SkippedBlock> blocks {
            { QStringLiteral("comment"), fragment(QStringLiteral(R"(^/')")),
              fragment(QStringLiteral(R"('/)")), false, false },
            { QStringLiteral("skinparam"), fragment(QStringLiteral(R"(^skinparam\b.*\{)")),
              QRegularExpression(), true, true },
            { QStringLiteral("legend"), fragment(QStringLiteral(R"(^legend\b)")),
              fragment(QStringLiteral(R"(end\s*legend)")), false, true },
            { QStringLiteral("style"), fragment(QStringLiteral(R"(^<style>)")),
              fragment(QStringLiteral(R"(</style>)")), false, true },
        };
        return blocks;
    }

    struct OpenBlock
    {
        const SkippedBlock* block = nullptr;
        int startLine = 0;
        int depth = 0;
    };

    int braceBalance(const QString& line)
    {
        return int(line.count(QLatin1Char('{'))) - int(line.count(QLatin1Char('}')));
    }

    class SequenceReader
    {
    public:
        Result<SequenceDiagram> read(const QString& text)
        {
            if (text.size() > kMaxLength)
                return Result<SequenceDiagram>::failure(
                    QStringLiteral("The diagram is longer than %1 characters").arg(kMaxLength));
            const QStringList lines = normalized(text).split(QLatin1Char('\n'));
            for (qsizetype index = 0; index < lines.size(); ++index)
            {
                m_lineNumber = int(index) + 1;
                if (const Error error = readLine(lines.at(index).trimmed()))
                    return Result<SequenceDiagram>::failure(*error);
                if (m_diagram.steps.size() > kMaxSteps)
                    return Result<SequenceDiagram>::failure(*lineError(
                        QStringLiteral("the diagram has more than %1 steps").arg(kMaxSteps)));
            }
            if (const Error error = finish())
                return Result<SequenceDiagram>::failure(*error);
            return Result<SequenceDiagram>::success(m_diagram);
        }

    private:
        Error readLine(const QString& line)
        {
            if (m_openBlock)
            {
                continueBlock(line);
                return std::nullopt;
            }
            if (m_openNote)
                return readNoteLine(line);
            if (line.isEmpty() || line.startsWith(QLatin1Char('\'')))
                return std::nullopt;
            if (const auto handled = readMessage(line))
                return *handled;
            if (startBlock(line))
                return std::nullopt;
            return readStatement(line);
        }

        Error readStatement(const QString& line)
        {
            static const QRegularExpression startEnd
                = statement(QStringLiteral(R"(@(start|end)uml\b.*)"));
            static const QRegularExpression endBox = statement(QStringLiteral(R"(end\s+box)"));
            static const QRegularExpression title = statement(QStringLiteral(R"(title\s+(.+))"));
            static const QRegularExpression divider
                = statement(QStringLiteral(R"(={2,}\s*(.*?)\s*={2,})"));
            static const QRegularExpression delay
                = statement(QStringLiteral(R"(\.\.\.\s*(.*?)\s*(?:\.\.\.)?)"));

            if (startEnd.match(line).hasMatch() || endBox.match(line).hasMatch())
                return std::nullopt;
            if (const auto match = title.match(line); match.hasMatch())
            {
                m_diagram.title = labelText(match.captured(1));
                return std::nullopt;
            }
            for (const auto& pattern : { divider, delay })
            {
                if (const auto match = pattern.match(line); match.hasMatch())
                {
                    m_diagram.steps.append(Divider { labelText(match.captured(1)) });
                    return std::nullopt;
                }
            }
            for (const auto reader :
                 { &SequenceReader::readParticipant, &SequenceReader::readReturn,
                   &SequenceReader::readNote, &SequenceReader::readGroup })
            {
                if (const auto handled = (this->*reader)(line))
                    return *handled;
            }
            if (readIgnored(line))
                return std::nullopt;
            return lineError(diagnosis(line));
        }

        Handled readParticipant(const QString& line)
        {
            static const QRegularExpression declaration = statement(
                QStringLiteral(
                    R"((participant|actor|database|entity|boundary|control|collections|queue))"
                    R"(\s+(%1)(?:\s+as\s+(%1))?(\s+<<[^>]*>>)?(\s+#\S+)?(?:\s+order\s+\d+)?\s*)")
                    .arg(kName));
            const auto match = declaration.match(line);
            if (!match.hasMatch())
                return std::nullopt;

            const QString name = match.captured(2);
            const QString alias = match.captured(3);
            QString id = unquoted(name);
            QString label = id;
            if (!alias.isEmpty())
            {
                id = isQuoted(alias) ? unquoted(name) : alias;
                label = isQuoted(alias) ? unquoted(alias) : unquoted(name);
            }
            if (!match.captured(4).isEmpty())
                warn(QStringLiteral("stereotype ignored"));
            if (!match.captured(5).isEmpty())
                warn(QStringLiteral("colour ignored"));

            const int index = participantIndex(id);
            m_diagram.participants[index].label = labelText(label);
            m_diagram.participants[index].kind = participantKind(match.captured(1));
            return Error {};
        }

        Handled readMessage(const QString& line)
        {
            static const QRegularExpression message
                = statement(QStringLiteral(R"((%1)\s*%2\s*(%1)\s*(\+\+|--|\*\*|!!)?\s*(?::(.*))?)")
                                .arg(kName, kArrow));
            static const QRegularExpression colour(QStringLiteral(R"(\[[^\]]*\])"));
            const auto match = message.match(line);
            if (!match.hasMatch())
                return std::nullopt;

            QString arrow = match.captured(2);
            if (arrow.contains(colour))
            {
                arrow.remove(colour);
                warn(QStringLiteral("arrow colour ignored"));
            }
            if (!match.captured(4).isEmpty())
                warn(QStringLiteral("activation ignored"));

            const bool reversed = arrow.startsWith(QLatin1Char('<'));
            const int left = participantIndex(unquoted(match.captured(1)));
            const int right = participantIndex(unquoted(match.captured(3)));

            Message sent;
            sent.from = reversed ? right : left;
            sent.to = reversed ? left : right;
            sent.label = labelText(match.captured(5));
            sent.line = arrow.contains(QStringLiteral("--")) ? LineStyle::Dashed : LineStyle::Solid;
            sent.head = arrow.contains(QStringLiteral(">>")) || arrow.contains(QStringLiteral("<<"))
                ? MessageHead::Open
                : MessageHead::Filled;
            trackCall(sent);
            appendMessage(sent);
            return Error {};
        }

        Handled readReturn(const QString& line)
        {
            static const QRegularExpression returning
                = statement(QStringLiteral(R"(return(?:\s+(.*))?)"));
            const auto match = returning.match(line);
            if (!match.hasMatch())
                return std::nullopt;
            if (m_openCalls.isEmpty())
                return lineError(QStringLiteral("\"return\" has no call to return from"));

            const Message call = m_openCalls.takeLast();
            Message reply;
            reply.from = call.to;
            reply.to = call.from;
            reply.label = labelText(match.captured(1));
            reply.line = LineStyle::Dashed;
            appendMessage(reply);
            return Error {};
        }

        Handled readNote(const QString& line)
        {
            static const QRegularExpression note = statement(
                QStringLiteral(
                    R"([hr]?note\s+(left|right|over)(?:(?:\s+of)?\s+(%1)(?:\s*,\s*(%1))?)?)"
                    R"((\s+#\S+)?\s*(?::(.*))?)")
                    .arg(kName));
            const auto match = note.match(line);
            if (!match.hasMatch())
                return std::nullopt;

            Note parsed;
            parsed.placement = notePlacement(match.captured(1));
            if (!match.captured(4).isEmpty())
                warn(QStringLiteral("colour ignored"));

            if (match.captured(2).isEmpty())
            {
                if (!m_lastMessage)
                    return lineError(
                        QStringLiteral("a note without a participant needs a message before it"));
                const int low = std::min(m_lastMessage->from, m_lastMessage->to);
                const int high = std::max(m_lastMessage->from, m_lastMessage->to);
                parsed.first = parsed.placement == NotePlacement::Right ? high : low;
                parsed.last = parsed.placement == NotePlacement::Left ? low : high;
            }
            else
            {
                parsed.first = participantIndex(unquoted(match.captured(2)));
                parsed.last = parsed.first;
                if (!match.captured(3).isEmpty())
                {
                    if (parsed.placement != NotePlacement::Over)
                        return lineError(
                            QStringLiteral("only \"note over\" can span participants"));
                    const int second = participantIndex(unquoted(match.captured(3)));
                    parsed.last = std::max(parsed.first, second);
                    parsed.first = std::min(parsed.first, second);
                }
            }

            if (match.hasCaptured(5))
            {
                parsed.text = labelText(match.captured(5));
                m_diagram.steps.append(parsed);
                return Error {};
            }
            m_openNote = parsed;
            m_openNoteLine = m_lineNumber;
            m_noteLines.clear();
            return Error {};
        }

        Error readNoteLine(const QString& line)
        {
            static const QRegularExpression endNote = statement(QStringLiteral(R"(end\s*note)"));
            if (!endNote.match(line).hasMatch())
            {
                m_noteLines.append(line);
                return std::nullopt;
            }
            Note finished = *m_openNote;
            finished.text = labelText(m_noteLines.join(QLatin1Char('\n')));
            m_diagram.steps.append(finished);
            m_openNote.reset();
            return std::nullopt;
        }

        Handled readGroup(const QString& line)
        {
            static const QRegularExpression start
                = statement(QStringLiteral(R"((%1)\b\s*(.*))").arg(kGroupKeywords));
            static const QRegularExpression otherwise
                = statement(QStringLiteral(R"(else\b\s*(.*))"));
            static const QRegularExpression end
                = statement(QStringLiteral(R"(end(?:\s+(?:%1))?)").arg(kGroupKeywords));

            if (const auto match = start.match(line); match.hasMatch())
            {
                const QString keyword = match.captured(1).toLower();
                m_diagram.steps.append(GroupStart { keyword, labelText(match.captured(2)) });
                m_openGroups.append({ m_lineNumber, keyword });
                return Error {};
            }
            if (const auto match = otherwise.match(line); match.hasMatch())
            {
                if (m_openGroups.isEmpty())
                    return lineError(QStringLiteral("\"else\" outside of a group"));
                m_diagram.steps.append(GroupElse { labelText(match.captured(1)) });
                return Error {};
            }
            if (end.match(line).hasMatch())
            {
                if (m_openGroups.isEmpty())
                    return lineError(QStringLiteral("\"end\" without an open group"));
                m_openGroups.removeLast();
                m_diagram.steps.append(GroupEnd {});
                return Error {};
            }
            return std::nullopt;
        }

        bool readIgnored(const QString& line)
        {
            static const QRegularExpression ignored = statement(QStringLiteral(
                R"(((skinparam|hide|show|autonumber|activate|deactivate|destroy|create|)"
                R"(autoactivate|scale|header|footer|caption|newpage|mainframe|box)\b.*)"
                R"(|!.*|\|\|(?:\d+\|)?\|))"));
            const auto match = ignored.match(line);
            if (!match.hasMatch())
                return false;
            const QString keyword
                = match.captured(2).isEmpty() ? line : match.captured(2).toLower();
            warn(QStringLiteral("\"%1\" ignored").arg(keyword));
            return true;
        }

        QString diagnosis(const QString& line) const
        {
            static const QRegularExpression twoWay = fragment(QStringLiteral(R"(<-+>)"));
            static const QRegularExpression outside
                = fragment(QStringLiteral(R"(^\[\s*[-<]|[->]\s*\]\s*(:|$))"));
            static const QRegularExpression reference = fragment(QStringLiteral(R"(^ref\b)"));
            static const QRegularExpression spacedName
                = fragment(QStringLiteral(R"(^[^"\s:]+(\s+[^"\s:-]+)+\s*(-+>|<-+))"));

            const QString unknown = QStringLiteral("unknown statement \"%1\"").arg(line);
            if (twoWay.match(line).hasMatch())
                return unknown
                    + QStringLiteral("; two-way arrows are not supported, write two messages");
            if (outside.match(line).hasMatch())
                return unknown
                    + QStringLiteral("; messages from outside ([-> or ->]) are not supported, "
                                     "name the sender");
            if (reference.match(line).hasMatch())
                return unknown
                    + QStringLiteral("; \"ref\" is not supported, use \"note over\" instead");
            if (spacedName.match(line).hasMatch())
                return unknown
                    + QStringLiteral("; names with spaces must be quoted, "
                                     "for example \"Web Browser\" -> Server");
            return unknown;
        }

        bool startBlock(const QString& line)
        {
            for (const SkippedBlock& block : skippedBlocks())
            {
                const auto match = block.start.match(line);
                if (!match.hasMatch())
                    continue;
                if (block.warns)
                    warn(QStringLiteral("\"%1\" block ignored").arg(block.name));
                OpenBlock open { &block, m_lineNumber, braceBalance(line) };
                const bool closed = block.countsBraces
                    ? open.depth <= 0
                    : block.end.match(line, match.capturedEnd()).hasMatch();
                if (!closed)
                    m_openBlock = open;
                return true;
            }
            return false;
        }

        void continueBlock(const QString& line)
        {
            if (m_openBlock->block->countsBraces)
            {
                m_openBlock->depth += braceBalance(line);
                if (m_openBlock->depth <= 0)
                    m_openBlock.reset();
                return;
            }
            if (m_openBlock->block->end.match(line).hasMatch())
                m_openBlock.reset();
        }

        Error finish()
        {
            if (m_openNote)
                return lineErrorAt(m_openNoteLine,
                                   QStringLiteral("note is not closed with \"end note\""));
            if (m_openBlock)
                return lineErrorAt(
                    m_openBlock->startLine,
                    QStringLiteral("\"%1\" block is not closed").arg(m_openBlock->block->name));
            if (!m_openGroups.isEmpty())
            {
                const auto& [line, keyword] = m_openGroups.last();
                return lineErrorAt(
                    line, QStringLiteral("\"%1\" is not closed with \"end\"").arg(keyword));
            }
            if (m_diagram.participants.isEmpty())
                return QStringLiteral("The diagram has no participants");
            return std::nullopt;
        }

        int participantIndex(const QString& id)
        {
            for (qsizetype index = 0; index < m_diagram.participants.size(); ++index)
            {
                if (m_diagram.participants.at(index).id == id)
                    return int(index);
            }
            m_diagram.participants.append(Participant { id, id, ParticipantKind::Box });
            return int(m_diagram.participants.size() - 1);
        }

        void trackCall(const Message& message)
        {
            if (message.from == message.to)
                return;
            if (message.line == LineStyle::Solid)
            {
                m_openCalls.append(message);
                return;
            }
            if (!m_openCalls.isEmpty() && m_openCalls.last().from == message.to
                && m_openCalls.last().to == message.from)
                m_openCalls.removeLast();
        }

        void appendMessage(const Message& message)
        {
            m_diagram.steps.append(message);
            m_lastMessage = message;
        }

        void warn(const QString& text)
        {
            m_diagram.warnings.append(QStringLiteral("Line %1: %2").arg(m_lineNumber).arg(text));
        }

        Error lineError(const QString& text) const { return lineErrorAt(m_lineNumber, text); }

        static Error lineErrorAt(int line, const QString& text)
        {
            return QStringLiteral("Line %1: %2").arg(line).arg(text);
        }

        SequenceDiagram m_diagram;
        int m_lineNumber = 0;
        std::optional<Message> m_lastMessage;
        QList<Message> m_openCalls;
        QList<std::pair<int, QString>> m_openGroups;
        std::optional<Note> m_openNote;
        int m_openNoteLine = 0;
        QStringList m_noteLines;
        std::optional<OpenBlock> m_openBlock;
    };
}

Result<SequenceDiagram> SequenceParser::parse(const QString& text)
{
    return SequenceReader().read(text);
}
}
