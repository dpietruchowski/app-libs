#include "activityparser.h"

#include "diagram/parsersupport.h"

#include <QRegularExpression>

#include <algorithm>
#include <optional>
#include <utility>

namespace diagram
{
namespace
{
    using parsing::Error;
    using parsing::fragment;
    using parsing::labelText;
    using parsing::statement;
    using Handled = std::optional<Error>;

    constexpr qsizetype kMaxElements = 300;

    enum class FrameKind
    {
        Root,
        If,
        While,
        Repeat,
        Fork,
        Partition
    };

    enum class Previous
    {
        Other,
        Action,
        Note
    };

    QString keywordOf(FrameKind kind)
    {
        switch (kind)
        {
            case FrameKind::If:
                return QStringLiteral("if");
            case FrameKind::While:
                return QStringLiteral("while");
            case FrameKind::Repeat:
                return QStringLiteral("repeat");
            case FrameKind::Fork:
                return QStringLiteral("fork");
            case FrameKind::Partition:
                return QStringLiteral("partition");
            case FrameKind::Root:
                break;
        }
        return QString();
    }

    QString closerOf(FrameKind kind)
    {
        switch (kind)
        {
            case FrameKind::If:
                return QStringLiteral("endif");
            case FrameKind::While:
                return QStringLiteral("endwhile");
            case FrameKind::Repeat:
                return QStringLiteral("repeat while (condition)");
            case FrameKind::Fork:
                return QStringLiteral("end fork");
            case FrameKind::Partition:
                return QStringLiteral("}");
            case FrameKind::Root:
                break;
        }
        return QString();
    }

    QString unquoted(const QString& text)
    {
        const QString trimmed = text.trimmed();
        if (trimmed.size() >= 2 && trimmed.startsWith(QLatin1Char('"'))
            && trimmed.endsWith(QLatin1Char('"')))
            return trimmed.mid(1, trimmed.size() - 2).trimmed();
        return trimmed;
    }

    struct Frame
    {
        FrameKind kind = FrameKind::Root;
        int line = 0;
        QString incomingLabel;
        ActivityBlock block;
        ActivityIf ifNode;
        ActivityWhile whileNode;
        ActivityRepeat repeatNode;
        ActivityFork forkNode;
        QString partitionLabel;
        bool elseSeen = false;
    };

    struct OpenAction
    {
        int line = 0;
        QStringList lines;
    };

    struct OpenNote
    {
        int line = 0;
        ActivityNoteSide side = ActivityNoteSide::Right;
        QStringList lines;
    };

    struct Unsupported
    {
        QRegularExpression pattern;
        QString feature;
        QString rewrite;
    };

    const QList<Unsupported>& unsupportedFeatures()
    {
        static const QList<Unsupported> features {
            { fragment(QStringLiteral(R"(^\|.*\|$)")), QStringLiteral("swimlanes"),
              QStringLiteral("Show who does what with a sequence diagram, or drop the lanes and "
                             "name the actor in each action") },
            { fragment(QStringLiteral(R"(^(switch\s*\(|case\s*\(|endswitch\b))")),
              QStringLiteral("switch"), QStringLiteral("Write it as if / elseif / else / endif") },
            { fragment(QStringLiteral(
                  R"re(\(\*\)|^"[^"]*"\s*-+(\[[^\]]*\])?-*>|^-+(\[[^\]]*\])?-*>\s*")re")),
              QStringLiteral("the old activity syntax"),
              QStringLiteral("Use the new syntax: start, :action;, if (...) then, stop") },
            { fragment(QStringLiteral(R"(^(goto\b|label\b|\(\w+\)$))")),
              QStringLiteral("jumps and connectors"),
              QStringLiteral("Restructure the flow with while or repeat") },
            { fragment(QStringLiteral(R"(^(split(\s+again)?|end\s*split)$)")),
              QStringLiteral("split"), QStringLiteral("Use fork / fork again / end fork") },
            { fragment(QStringLiteral(R"(^break$)")), QStringLiteral("break"),
              QStringLiteral("End the branch with detach, or move the exit into the loop "
                             "condition") },
            { fragment(QStringLiteral(R"(^backward\s*:)")), QStringLiteral("backward actions"),
              QStringLiteral("Move the action into the repeat body") },
            { fragment(QStringLiteral(R"(^(else)?if\s*\(.*\)\s*(is|equals)\b)")),
              QStringLiteral("is/equals conditions"),
              QStringLiteral("Write the whole condition in the parentheses: if (a = b?) then "
                             "(yes)") },
            { fragment(QStringLiteral(R"(^floating\s+note\b)")), QStringLiteral("floating notes"),
              QStringLiteral("Attach the note to an action: put it right after the action") },
        };
        return features;
    }

    struct Form
    {
        QRegularExpression pattern;
        QString keyword;
        QString form;
    };

    const QList<Form>& knownForms()
    {
        static const QList<Form> forms {
            { fragment(QStringLiteral(R"(^if\b)")), QStringLiteral("if"),
              QStringLiteral("if (condition) then (yes)") },
            { fragment(QStringLiteral(R"(^elseif\b)")), QStringLiteral("elseif"),
              QStringLiteral("elseif (condition) then (yes)") },
            { fragment(QStringLiteral(R"(^else\b)")), QStringLiteral("else"),
              QStringLiteral("else (no)") },
            { fragment(QStringLiteral(R"(^end\s*if\b)")), QStringLiteral("endif"),
              QStringLiteral("endif") },
            { fragment(QStringLiteral(R"(^while\b)")), QStringLiteral("while"),
              QStringLiteral("while (condition) is (yes)") },
            { fragment(QStringLiteral(R"(^end\s*while\b)")), QStringLiteral("endwhile"),
              QStringLiteral("endwhile (no)") },
            { fragment(QStringLiteral(R"(^repeat\b)")), QStringLiteral("repeat"),
              QStringLiteral("repeat, closed later by repeat while (condition) is (yes) not "
                             "(no)") },
            { fragment(QStringLiteral(R"(^partition\b)")), QStringLiteral("partition"),
              QStringLiteral("partition Name {") },
            { fragment(QStringLiteral(R"(^fork\b)")), QStringLiteral("fork"),
              QStringLiteral("fork") },
            { fragment(QStringLiteral(R"(^note\b)")), QStringLiteral("note"),
              QStringLiteral("note left : text or note right : text") },
            { fragment(QStringLiteral(R"(^start\b)")), QStringLiteral("start"),
              QStringLiteral("start") },
            { fragment(QStringLiteral(R"(^stop\b)")), QStringLiteral("stop"),
              QStringLiteral("stop") },
            { fragment(QStringLiteral(R"(^detach\b)")), QStringLiteral("detach"),
              QStringLiteral("detach") },
            { fragment(QStringLiteral(R"(^kill\b)")), QStringLiteral("kill"),
              QStringLiteral("kill") },
            { fragment(QStringLiteral(R"(^-(\[[^\]]*\])?-*>)")), QStringLiteral("connector"),
              QStringLiteral("-> label;") },
        };
        return forms;
    }

    class ActivityReader
    {
    public:
        Result<ActivityDiagram> read(const QString& text)
        {
            if (text.size() > parsing::kMaxLength)
                return Result<ActivityDiagram>::failure(parsing::tooLongMessage());
            const QStringList lines = parsing::normalized(text).split(QLatin1Char('\n'));
            m_frames.push_back(Frame {});
            for (qsizetype index = 0; index < lines.size(); ++index)
            {
                m_lineNumber = int(index) + 1;
                const QString line = lines.at(index).trimmed();
                if (!line.isEmpty())
                    m_lastContentLine = m_lineNumber;
                if (const Error error = readLine(line))
                    return Result<ActivityDiagram>::failure(*error);
                if (m_elementCount > kMaxElements)
                    return Result<ActivityDiagram>::failure(*lineError(
                        QStringLiteral("the diagram has more than %1 elements").arg(kMaxElements)));
            }
            if (const Error error = finish())
                return Result<ActivityDiagram>::failure(*error);
            m_diagram.body = std::move(m_frames.front().block);
            return Result<ActivityDiagram>::success(m_diagram);
        }

    private:
        Error readLine(const QString& line)
        {
            if (m_skippedBlocks.isOpen())
            {
                m_skippedBlocks.continueWith(line);
                return std::nullopt;
            }
            if (m_openAction)
                return readActionLine(line);
            if (m_openNote)
                return readNoteLine(line);
            if (line.isEmpty() || line.startsWith(QLatin1Char('\'')))
                return std::nullopt;
            if (const auto warning = m_skippedBlocks.start(line, m_lineNumber))
            {
                if (!warning->isEmpty())
                    warn(*warning);
                return std::nullopt;
            }
            return readStatement(line);
        }

        Error readStatement(const QString& line)
        {
            static const QRegularExpression title = statement(QStringLiteral(R"(title\s+(.+))"));

            m_before = std::exchange(m_previous, Previous::Other);
            if (parsing::isDiagramMarker(line))
            {
                m_previous = m_before;
                return std::nullopt;
            }
            if (const auto match = title.match(line); match.hasMatch())
            {
                m_diagram.title = labelText(match.captured(1));
                m_previous = m_before;
                return std::nullopt;
            }
            for (const auto reader :
                 { &ActivityReader::readConnector, &ActivityReader::readUnsupported,
                   &ActivityReader::readFlowEnd, &ActivityReader::readAction,
                   &ActivityReader::readIf, &ActivityReader::readLoop, &ActivityReader::readFork,
                   &ActivityReader::readPartition, &ActivityReader::readNote,
                   &ActivityReader::readIgnored })
            {
                if (const auto handled = (this->*reader)(line))
                    return *handled;
            }
            return lineError(diagnosis(line));
        }

        Handled readConnector(const QString& line)
        {
            static const QRegularExpression connector
                = statement(QStringLiteral(R"((-(\[[^\]]*\])?-*>)\s*(.*?)\s*;)"));
            const auto match = connector.match(line);
            if (!match.hasMatch())
                return std::nullopt;
            if (!match.captured(2).isEmpty())
                warn(QStringLiteral("arrow colour ignored"));
            discardPendingLabel();
            m_pendingLabel = labelText(match.captured(3));
            m_pendingLabelLine = m_lineNumber;
            return Error {};
        }

        Handled readUnsupported(const QString& line)
        {
            for (const Unsupported& unsupported : unsupportedFeatures())
            {
                if (unsupported.pattern.match(line).hasMatch())
                    return lineError(parsing::unsupportedFeature(line, unsupported.feature,
                                                                 unsupported.rewrite));
            }
            return std::nullopt;
        }

        Handled readFlowEnd(const QString& line)
        {
            static const QRegularExpression flow
                = statement(QStringLiteral(R"((start|stop|end|detach|kill))"));
            const auto match = flow.match(line);
            if (!match.hasMatch())
                return std::nullopt;
            const QString keyword = match.captured(1).toLower();
            if (keyword == QStringLiteral("start"))
                append(ActivityStart {});
            else if (keyword == QStringLiteral("stop"))
                append(ActivityStop { FlowEnd::Stop });
            else if (keyword == QStringLiteral("end"))
                append(ActivityStop { FlowEnd::End });
            else if (keyword == QStringLiteral("detach"))
                append(ActivityBranchEnd { BranchEndKind::Detach });
            else
                append(ActivityBranchEnd { BranchEndKind::Kill });
            return Error {};
        }

        Handled readAction(const QString& line)
        {
            static const QRegularExpression action
                = statement(QStringLiteral(R"((#[^\s:]*)?:(.*))"));
            const auto match = action.match(line);
            if (!match.hasMatch())
                return std::nullopt;
            return startAction(match.captured(1), match.captured(2));
        }

        Error startAction(const QString& colour, const QString& firstLine)
        {
            if (!colour.isEmpty())
                warn(QStringLiteral("colour ignored"));
            m_openAction = OpenAction { m_lineNumber, {} };
            return readActionLine(firstLine);
        }

        Error readActionLine(const QString& piece)
        {
            static const QRegularExpression terminated
                = fragment(QStringLiteral(R"(^(.*?)\s*([;<>|/\]}])$)"));
            const QString trimmed = piece.trimmed();
            const auto match = terminated.match(trimmed);
            if (!match.hasMatch())
            {
                if (!trimmed.isEmpty())
                    m_openAction->lines.append(trimmed);
                return std::nullopt;
            }
            const QString body = match.captured(1).trimmed();
            if (!body.isEmpty())
                m_openAction->lines.append(body);
            const QString terminator = match.captured(2);
            if (terminator != QLatin1String(";"))
                warnAt(m_openAction->line,
                       QStringLiteral("SDL shape \"%1\" drawn as a plain action").arg(terminator));
            ActivityAction action;
            action.text = labelText(m_openAction->lines.join(QLatin1Char('\n')));
            const int startLine = m_openAction->line;
            m_openAction.reset();
            append(std::move(action), startLine);
            m_previous = Previous::Action;
            return std::nullopt;
        }

        Handled readNote(const QString& line)
        {
            static const QRegularExpression note
                = statement(QStringLiteral(R"(note\s+(left|right)(\s+#\S+)?\s*(?::(.*))?)"));
            const auto match = note.match(line);
            if (!match.hasMatch())
                return std::nullopt;
            if (m_before == Previous::Note)
                return lineError(QStringLiteral(
                    "an action can carry only one note. Merge the two notes into one."));
            if (m_before != Previous::Action)
                return lineError(QStringLiteral(
                    "a note must follow an action; real PlantUML would silently drop this one. "
                    "Move it right after the action it describes."));
            if (!match.captured(2).isEmpty())
                warn(QStringLiteral("colour ignored"));

            const bool left
                = match.captured(1).compare(QStringLiteral("left"), Qt::CaseInsensitive) == 0;
            const ActivityNoteSide side = left ? ActivityNoteSide::Left : ActivityNoteSide::Right;
            if (match.hasCaptured(3))
            {
                attachNote(side, labelText(match.captured(3)));
                return Error {};
            }
            m_openNote = OpenNote { m_lineNumber, side, {} };
            return Error {};
        }

        Error readNoteLine(const QString& line)
        {
            static const QRegularExpression endNote = statement(QStringLiteral(R"(end\s*note)"));
            if (!endNote.match(line).hasMatch())
            {
                m_openNote->lines.append(line);
                return std::nullopt;
            }
            const OpenNote finished = *m_openNote;
            m_openNote.reset();
            attachNote(finished.side, labelText(finished.lines.join(QLatin1Char('\n'))));
            return std::nullopt;
        }

        void attachNote(ActivityNoteSide side, const QString& text)
        {
            auto& action = std::get<ActivityAction>(m_frames.back().block.back().node);
            action.note = ActivityNote { side, text };
            m_previous = Previous::Note;
        }

        Handled readIf(const QString& line)
        {
            static const QRegularExpression start
                = statement(QStringLiteral(R"(if\s*\((.*?)\)\s*then(?:\s*\((.*)\))?)"));
            static const QRegularExpression elseIf
                = statement(QStringLiteral(R"(elseif\s*\((.*?)\)\s*then(?:\s*\((.*)\))?)"));
            static const QRegularExpression otherwise
                = statement(QStringLiteral(R"(else(?:\s*\((.*)\))?)"));
            static const QRegularExpression end = statement(QStringLiteral(R"(end\s*if)"));

            if (const auto match = start.match(line); match.hasMatch())
            {
                Frame& frame = openFrame(FrameKind::If);
                frame.ifNode.branches.push_back(
                    { labelText(match.captured(1)), labelText(match.captured(2)), false, {} });
                return Error {};
            }
            if (const auto match = elseIf.match(line); match.hasMatch())
            {
                if (const Error error = requireOpen(FrameKind::If, QStringLiteral("elseif")))
                    return error;
                Frame& frame = m_frames.back();
                if (frame.elseSeen)
                    return lineError(elseAfterElse(QStringLiteral("elseif"), frame));
                nextBranch(
                    frame,
                    { labelText(match.captured(1)), labelText(match.captured(2)), false, {} });
                return Error {};
            }
            if (const auto match = otherwise.match(line); match.hasMatch())
            {
                if (const Error error = requireOpen(FrameKind::If, QStringLiteral("else")))
                    return error;
                Frame& frame = m_frames.back();
                if (frame.elseSeen)
                    return lineError(elseAfterElse(QStringLiteral("else"), frame));
                frame.elseSeen = true;
                nextBranch(frame, { QString(), labelText(match.captured(1)), true, {} });
                return Error {};
            }
            if (end.match(line).hasMatch())
            {
                if (const Error error = requireOpen(FrameKind::If, QStringLiteral("endif")))
                    return error;
                Frame frame = closeFrame();
                frame.ifNode.branches.back().block = std::move(frame.block);
                appendClosed(frame, std::move(frame.ifNode));
                return Error {};
            }
            return std::nullopt;
        }

        QString elseAfterElse(const QString& shown, const Frame& frame) const
        {
            return QStringLiteral("\"%1\" after \"else\" in the \"if\" opened at line %2. An "
                                  "\"if\" has one \"else\", and it comes last; use \"elseif\" for "
                                  "more conditions.")
                .arg(shown)
                .arg(frame.line);
        }

        void nextBranch(Frame& frame, ActivityIfBranch branch)
        {
            discardPendingLabel();
            frame.ifNode.branches.back().block = std::move(frame.block);
            frame.block.clear();
            frame.ifNode.branches.push_back(std::move(branch));
        }

        Handled readLoop(const QString& line)
        {
            static const QRegularExpression whileStart
                = statement(QStringLiteral(R"(while\s*\((.*?)\)(?:\s*is\s*\((.*)\))?)"));
            static const QRegularExpression whileEnd
                = statement(QStringLiteral(R"(end\s*while(?:\s*\((.*)\))?)"));
            static const QRegularExpression repeatStart
                = statement(QStringLiteral(R"(repeat(?:\s+((?:#[^\s:]*)?:.*))?)"));
            static const QRegularExpression repeatEnd = statement(QStringLiteral(
                R"(repeat\s*while\s*\((.*?)\)(?:\s*is\s*\((.*?)\))?(?:\s*not\s*\((.*?)\))?)"));

            if (const auto match = whileStart.match(line); match.hasMatch())
            {
                Frame& frame = openFrame(FrameKind::While);
                frame.whileNode.condition = labelText(match.captured(1));
                frame.whileNode.yesLabel = labelText(match.captured(2));
                return Error {};
            }
            if (const auto match = whileEnd.match(line); match.hasMatch())
            {
                if (const Error error = requireOpen(FrameKind::While, QStringLiteral("endwhile")))
                    return error;
                discardPendingLabel();
                Frame frame = closeFrame();
                frame.whileNode.block = std::move(frame.block);
                frame.whileNode.exitLabel = labelText(match.captured(1));
                appendClosed(frame, std::move(frame.whileNode));
                return Error {};
            }
            if (const auto match = repeatEnd.match(line); match.hasMatch())
            {
                if (const Error error
                    = requireOpen(FrameKind::Repeat, QStringLiteral("repeat while")))
                    return error;
                discardPendingLabel();
                Frame frame = closeFrame();
                frame.repeatNode.block = std::move(frame.block);
                frame.repeatNode.condition = labelText(match.captured(1));
                frame.repeatNode.yesLabel = labelText(match.captured(2));
                frame.repeatNode.exitLabel = labelText(match.captured(3));
                appendClosed(frame, std::move(frame.repeatNode));
                return Error {};
            }
            if (const auto match = repeatStart.match(line); match.hasMatch())
            {
                openFrame(FrameKind::Repeat);
                if (match.captured(1).isEmpty())
                    return Error {};
                return readAction(match.captured(1));
            }
            return std::nullopt;
        }

        Handled readFork(const QString& line)
        {
            static const QRegularExpression start = statement(QStringLiteral(R"(fork)"));
            static const QRegularExpression again = statement(QStringLiteral(R"(fork\s+again)"));
            static const QRegularExpression end
                = statement(QStringLiteral(R"(end\s+(?:fork|merge)(?:\s*\{.*\})?)"));

            if (start.match(line).hasMatch())
            {
                openFrame(FrameKind::Fork);
                return Error {};
            }
            if (again.match(line).hasMatch())
            {
                if (const Error error = requireOpen(FrameKind::Fork, QStringLiteral("fork again")))
                    return error;
                discardPendingLabel();
                Frame& frame = m_frames.back();
                frame.forkNode.branches.push_back(std::move(frame.block));
                frame.block.clear();
                return Error {};
            }
            if (end.match(line).hasMatch())
            {
                if (const Error error = requireOpen(FrameKind::Fork, QStringLiteral("end fork")))
                    return error;
                discardPendingLabel();
                Frame frame = closeFrame();
                frame.forkNode.branches.push_back(std::move(frame.block));
                appendClosed(frame, std::move(frame.forkNode));
                return Error {};
            }
            return std::nullopt;
        }

        Handled readPartition(const QString& line)
        {
            static const QRegularExpression start
                = statement(QStringLiteral(R"(partition\s+(.+?)\s*\{)"));
            static const QRegularExpression end = statement(QStringLiteral(R"(\})"));

            if (const auto match = start.match(line); match.hasMatch())
            {
                Frame& frame = openFrame(FrameKind::Partition);
                frame.partitionLabel = labelText(unquoted(match.captured(1)));
                return Error {};
            }
            if (end.match(line).hasMatch())
            {
                if (const Error error = requireOpen(FrameKind::Partition, QStringLiteral("}")))
                    return error;
                discardPendingLabel();
                Frame frame = closeFrame();
                appendClosed(frame,
                             ActivityPartition { frame.partitionLabel, std::move(frame.block) });
                return Error {};
            }
            return std::nullopt;
        }

        Handled readIgnored(const QString& line)
        {
            static const QRegularExpression ignored = statement(
                QStringLiteral(R"(((skinparam|hide|show|scale|header|footer|caption)\b.*|!.*))"));
            const auto match = ignored.match(line);
            if (!match.hasMatch())
                return std::nullopt;
            const QString keyword
                = match.captured(2).isEmpty() ? line : match.captured(2).toLower();
            warn(QStringLiteral("\"%1\" ignored").arg(keyword));
            m_previous = m_before;
            return Error {};
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
            if (line.endsWith(QLatin1Char(';')))
                return QStringLiteral("\"%1\" looks like an action without its leading colon. "
                                      "Write it as \":%1\".")
                    .arg(line);
            return QStringLiteral(
                       "unknown statement \"%1\". An activity diagram here understands: start, "
                       "stop, end, :action;, -> label;, if/elseif/else/endif, while/endwhile, "
                       "repeat/repeat while, fork/fork again/end fork, partition Name { }, "
                       "note left|right, detach, kill.")
                .arg(line);
        }

        Frame& openFrame(FrameKind kind)
        {
            Frame frame;
            frame.kind = kind;
            frame.line = m_lineNumber;
            frame.incomingLabel = takePendingLabel();
            ++m_elementCount;
            m_frames.push_back(std::move(frame));
            return m_frames.back();
        }

        Frame closeFrame()
        {
            Frame frame = std::move(m_frames.back());
            m_frames.pop_back();
            return frame;
        }

        void appendClosed(const Frame& frame, ActivityNode node)
        {
            m_frames.back().block.push_back(
                ActivityElement { std::move(node), frame.incomingLabel, frame.line });
        }

        Error requireOpen(FrameKind expected, const QString& shown) const
        {
            const Frame& top = m_frames.back();
            if (top.kind == expected)
                return std::nullopt;
            const bool openSomewhere
                = std::any_of(m_frames.cbegin(), m_frames.cend(),
                              [expected](const Frame& frame) { return frame.kind == expected; });
            if (!openSomewhere)
                return lineError(QStringLiteral("\"%1\" without an open \"%2\"")
                                     .arg(shown, keywordOf(expected)));
            return lineError(QStringLiteral("\"%1\" does not match the \"%2\" opened at line %3. "
                                            "Close it first with \"%4\".")
                                 .arg(shown, keywordOf(top.kind))
                                 .arg(top.line)
                                 .arg(closerOf(top.kind)));
        }

        template <typename Node> void append(Node node, int line = 0)
        {
            m_frames.back().block.push_back(ActivityElement { ActivityNode(std::move(node)),
                                                              takePendingLabel(),
                                                              line > 0 ? line : m_lineNumber });
            ++m_elementCount;
        }

        QString takePendingLabel() { return std::exchange(m_pendingLabel, QString()); }

        void discardPendingLabel()
        {
            if (m_pendingLabel.isEmpty())
                return;
            warnAt(m_pendingLabelLine,
                   QStringLiteral("connector label \"%1\" has no element after it, ignored")
                       .arg(m_pendingLabel));
            m_pendingLabel.clear();
        }

        Error finish()
        {
            if (m_openAction)
                return lineErrorAt(m_lastContentLine,
                                   QStringLiteral("the action started at line %1 has no closing "
                                                  "\";\". End the action text with \";\".")
                                       .arg(m_openAction->line));
            if (m_openNote)
                return lineErrorAt(m_lastContentLine,
                                   QStringLiteral("the note started at line %1 has no closing "
                                                  "\"end note\". Add \"end note\" after the text.")
                                       .arg(m_openNote->line));
            if (const auto unclosed = m_skippedBlocks.unclosed())
                return lineErrorAt(
                    unclosed->first,
                    QStringLiteral("\"%1\" block is not closed").arg(unclosed->second));
            if (m_frames.size() > 1)
            {
                const Frame& open = m_frames.back();
                return lineErrorAt(
                    m_lastContentLine,
                    QStringLiteral("the \"%1\" opened at line %2 is not closed with \"%3\". Add "
                                   "\"%3\" where the block ends.")
                        .arg(keywordOf(open.kind))
                        .arg(open.line)
                        .arg(closerOf(open.kind)));
            }
            discardPendingLabel();
            if (m_elementCount == 0)
                return QStringLiteral("The diagram has no elements");
            return std::nullopt;
        }

        void warn(const QString& text) { warnAt(m_lineNumber, text); }

        void warnAt(int line, const QString& text)
        {
            m_diagram.warnings.append(parsing::lineErrorText(line, text));
        }

        Error lineError(const QString& text) const { return lineErrorAt(m_lineNumber, text); }

        static Error lineErrorAt(int line, const QString& text)
        {
            return parsing::lineErrorText(line, text);
        }

        ActivityDiagram m_diagram;
        std::vector<Frame> m_frames;
        int m_lineNumber = 0;
        int m_lastContentLine = 0;
        qsizetype m_elementCount = 0;
        Previous m_previous = Previous::Other;
        Previous m_before = Previous::Other;
        QString m_pendingLabel;
        int m_pendingLabelLine = 0;
        std::optional<OpenAction> m_openAction;
        std::optional<OpenNote> m_openNote;
        parsing::SkippedBlocks m_skippedBlocks;
    };
}

Result<ActivityDiagram> ActivityParser::parse(const QString& text)
{
    return ActivityReader().read(text);
}
}
