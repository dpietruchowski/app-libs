#include "stateparser.h"

#include "diagram/parsersupport.h"

#include <QRegularExpression>
#include <QSet>

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

    constexpr qsizetype kMaxElements = 300;

    const QString kName = QStringLiteral(R"((?:"[^"]+"|\w+))");
    const QString kEndpoint = QStringLiteral(R"((?:\[\*\]|"[^"]+"|\w+))");
    const QString kInitialOrFinal = QStringLiteral("[*]");

    const QString kUnderstands = QStringLiteral(
        "A state diagram here understands: [*] --> A, A --> B and A -> B with an optional : "
        "label, state \"Long name\" as Alias (add { to hold other states, close it with }), "
        "Name : description, note left of/right of Name : text, title");

    QString unquoted(const QString& token)
    {
        if (token.size() >= 2 && token.startsWith(QLatin1Char('"'))
            && token.endsWith(QLatin1Char('"')))
            return token.mid(1, token.size() - 2).trimmed();
        return token;
    }

    bool isQuoted(const QString& token) { return token.startsWith(QLatin1Char('"')); }

    struct Unsupported
    {
        QRegularExpression pattern;
        QString feature;
        QString hint;
    };

    const QList<Unsupported>& unsupportedDirectives()
    {
        static const QList<Unsupported> directives {
            { fragment(QStringLiteral(R"(^skinparam\b)")), QStringLiteral("skinparam"),
              QStringLiteral("Remove the line; the app colours every state itself") },
            { fragment(QStringLiteral(R"(^(hide|show)\b)")),
              QStringLiteral("\"hide\" and \"show\""), QStringLiteral("Remove the line") },
            { fragment(QStringLiteral(R"(^(left\s+to\s+right|top\s+to\s+bottom)\s+direction$)")),
              QStringLiteral("layout directions"),
              QStringLiteral("Remove the line; the states are placed top to bottom") },
        };
        return directives;
    }

    const QList<Unsupported>& unsupportedForms()
    {
        static const QList<Unsupported> forms {
            { fragment(QStringLiteral(R"(^(--+|\|\|+)$)")), QStringLiteral("concurrent regions"),
              QStringLiteral("Draw each region as a state of its own or as a separate diagram") },
            { fragment(QStringLiteral(R"(\[H\*?\])")), QStringLiteral("history states"),
              QStringLiteral("Draw an ordinary transition back to the state instead") },
            { fragment(QStringLiteral(R"(^state\b.*<<)")),
              QStringLiteral("stereotypes such as <<choice>>, <<fork>>, <<join>> and <<end>>"),
              QStringLiteral("Draw the state without the stereotype") },
            { fragment(QStringLiteral(
                  R"re(^state\s+(?:"[^"]*"|\w+)(?:\s+as\s+(?:"[^"]*"|\w+))?\s+#\S+)re")),
              QStringLiteral("state colours"),
              QStringLiteral("Remove the colour; the app colours every state itself") },
            { fragment(QStringLiteral(R"(^[hr]?note\s+(top|bottom)\b)")),
              QStringLiteral("notes above or below a state"),
              QStringLiteral("Use note left of Name : text or note right of Name : text") },
            { fragment(QStringLiteral(R"(^(floating\s+note\b|[hr]?note\s+on\s+link\b))")),
              QStringLiteral("floating notes and notes on transitions"),
              QStringLiteral("Attach the note to a state: note left of Name : text") },
            { fragment(QStringLiteral(
                  R"re(^(?:"[^"]+"|\w+|\[\*\])\s+\S*[-.=]\S*\s+(?:"[^"]+"|\w+|\[\*\]))re")),
              QStringLiteral("this kind of arrow"),
              QStringLiteral("Write the transition as A --> B or A -> B") },
        };
        return forms;
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
            { fragment(QStringLiteral(R"(^state\b)")), QStringLiteral("state"),
              QStringLiteral("state \"Long name\" as Alias, or state Name { to open a composite "
                             "state") },
            { fragment(QStringLiteral(R"(^note\b)")), QStringLiteral("note"),
              QStringLiteral("note left of Name : text or note right of Name : text") },
            { fragment(QStringLiteral(R"(^title\b)")), QStringLiteral("title"),
              QStringLiteral("title Text") },
        };
        return forms;
    }

    struct Composite
    {
        int state = -1;
        int line = 0;
    };

    struct OpenNote
    {
        int line = 0;
        int state = -1;
        StateNoteSide side = StateNoteSide::Right;
        QStringList lines;
    };

    class StateReader
    {
    public:
        Result<StateDiagram> read(const QString& text)
        {
            if (text.size() > parsing::kMaxLength)
                return Result<StateDiagram>::failure(parsing::tooLongMessage());
            const QStringList lines = parsing::normalized(text).split(QLatin1Char('\n'));
            for (qsizetype index = 0; index < lines.size(); ++index)
            {
                m_lineNumber = int(index) + 1;
                const QString line = lines.at(index).trimmed();
                if (!line.isEmpty())
                    m_lastContentLine = m_lineNumber;
                if (const Error error = readLine(line))
                    return Result<StateDiagram>::failure(*error);
                if (elementCount() > kMaxElements)
                    return Result<StateDiagram>::failure(*lineError(
                        QStringLiteral("the diagram has more than %1 elements").arg(kMaxElements)));
            }
            if (const Error error = finish())
                return Result<StateDiagram>::failure(*error);
            return Result<StateDiagram>::success(m_diagram);
        }

    private:
        qsizetype elementCount() const
        {
            return m_diagram.states.size() + m_diagram.transitions.size() + m_diagram.notes.size();
        }

        Error readLine(const QString& line)
        {
            if (m_skippedBlocks.isOpen())
            {
                m_skippedBlocks.continueWith(line);
                return std::nullopt;
            }
            if (m_openNote)
                return readNoteLine(line);
            if (line.isEmpty() || line.startsWith(QLatin1Char('\'')))
                return std::nullopt;
            if (const auto handled = readUnsupported(unsupportedDirectives(), line))
                return *handled;
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

            if (parsing::isDiagramMarker(line))
                return std::nullopt;
            if (const auto match = title.match(line); match.hasMatch())
            {
                m_diagram.title = labelText(match.captured(1));
                return std::nullopt;
            }
            for (const auto reader : { &StateReader::readClose, &StateReader::readDeclaration,
                                       &StateReader::readTransition, &StateReader::readNote,
                                       &StateReader::readDescription, &StateReader::readIgnored,
                                       &StateReader::readUnsupportedForm })
            {
                if (const auto handled = (this->*reader)(line))
                    return *handled;
            }
            return lineError(diagnosis(line));
        }

        Handled readClose(const QString& line)
        {
            static const QRegularExpression close = statement(QStringLiteral(R"(\})"));
            if (!close.match(line).hasMatch())
                return std::nullopt;
            if (m_composites.isEmpty())
                return lineError(QStringLiteral("\"}\" without an open composite state"));
            m_composites.removeLast();
            return Error {};
        }

        Handled readDeclaration(const QString& line)
        {
            static const QRegularExpression declaration
                = statement(QStringLiteral(R"(state\s+(%1)(?:\s+as\s+(%1))?\s*(\{)?)").arg(kName));
            const auto match = declaration.match(line);
            if (!match.hasMatch())
                return std::nullopt;

            const QString name = match.captured(1);
            const QString alias = match.captured(2);
            QString id = unquoted(name);
            QString label = unquoted(name);
            if (!alias.isEmpty())
            {
                id = isQuoted(alias) ? unquoted(name) : alias;
                label = isQuoted(alias) ? unquoted(alias) : unquoted(name);
            }
            label = labelText(label);
            const bool opens = !match.captured(3).isEmpty();

            int index = indexOf(id);
            if (index >= 0 && m_declared.contains(index))
                return lineError(QStringLiteral("the state \"%1\" is already declared at line %2. "
                                                "Give each state its own name.")
                                     .arg(id)
                                     .arg(m_diagram.states.at(index).line));
            if (index < 0)
            {
                index = appendState(id, label);
            }
            else
            {
                m_diagram.states[index].label = label;
                m_diagram.states[index].line = m_lineNumber;
            }
            m_declared.insert(index);
            m_diagram.states[index].isComposite = opens;
            if (opens)
                m_composites.append({ index, m_lineNumber });
            return Error {};
        }

        Handled readTransition(const QString& line)
        {
            static const QRegularExpression transition = statement(
                QStringLiteral(
                    R"((%1)\s*(-+>|-(?:up|down|left|right|u|d|l|r)-+>)\s*(%1)\s*(?::(.*))?)")
                    .arg(kEndpoint));
            const auto match = transition.match(line);
            if (!match.hasMatch())
                return std::nullopt;

            const QString source = match.captured(1);
            const QString target = match.captured(3);
            if (source == kInitialOrFinal && target == kInitialOrFinal)
                return lineError(QStringLiteral("the transition \"%1\" goes from [*] to [*]. Give "
                                                "it a state on one side.")
                                     .arg(line));
            const int from = resolve(source, StateKind::Initial);
            const int to = resolve(target, StateKind::Final);

            static const QRegularExpression direction(QStringLiteral("[a-z]"));
            if (match.captured(2).contains(direction))
                warn(QStringLiteral("direction hint ignored"));
            StateTransition parsed;
            parsed.from = from;
            parsed.to = to;
            parsed.label = labelText(match.captured(4));
            parsed.sourceLine = m_lineNumber;
            m_diagram.transitions.append(parsed);
            return Error {};
        }

        Handled readNote(const QString& line)
        {
            static const QRegularExpression note = statement(
                QStringLiteral(R"([hr]?note\s+(left|right)\s+of\s+(%1)(\s+#\S+)?\s*(?::(.*))?)")
                    .arg(kName));
            const auto match = note.match(line);
            if (!match.hasMatch())
                return std::nullopt;

            const QString id = unquoted(match.captured(2));
            const int state = indexOf(id);
            if (state < 0)
                return lineError(QStringLiteral("unknown state \"%1\" in the note. Declare it "
                                                "before the note.")
                                     .arg(id));
            if (!match.captured(3).isEmpty())
                warn(QStringLiteral("colour ignored"));
            const bool left
                = match.captured(1).compare(QStringLiteral("left"), Qt::CaseInsensitive) == 0;
            const StateNoteSide side = left ? StateNoteSide::Left : StateNoteSide::Right;
            if (hasNote(state, side))
                return lineError(
                    QStringLiteral("the state \"%1\" already has a note on the %2. "
                                   "Merge the two notes into one.")
                        .arg(id, left ? QStringLiteral("left") : QStringLiteral("right")));
            if (match.hasCaptured(4))
            {
                m_diagram.notes.append({ state, side, labelText(match.captured(4)) });
                return Error {};
            }
            m_openNote = OpenNote { m_lineNumber, state, side, {} };
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
            m_diagram.notes.append({ finished.state, finished.side,
                                     labelText(finished.lines.join(QLatin1Char('\n'))) });
            return std::nullopt;
        }

        Handled readDescription(const QString& line)
        {
            static const QRegularExpression description
                = statement(QStringLiteral(R"((%1)\s*:\s*(.+))").arg(kName));
            const auto match = description.match(line);
            if (!match.hasMatch())
                return std::nullopt;

            const int state = resolve(unquoted(match.captured(1)), StateKind::Normal);
            const QStringList lines = labelText(match.captured(2)).split(QLatin1Char('\n'));
            for (const QString& text : lines)
            {
                if (!text.trimmed().isEmpty())
                    m_diagram.states[state].descriptions.append(text.trimmed());
            }
            return Error {};
        }

        Handled readIgnored(const QString& line)
        {
            static const QRegularExpression ignored
                = statement(QStringLiteral(R"(((scale|header|footer|caption)\b.*|!.*))"));
            const auto match = ignored.match(line);
            if (!match.hasMatch())
                return std::nullopt;
            const QString keyword
                = match.captured(2).isEmpty() ? line : match.captured(2).toLower();
            warn(QStringLiteral("\"%1\" ignored").arg(keyword));
            return Error {};
        }

        Handled readUnsupportedForm(const QString& line)
        {
            return readUnsupported(unsupportedForms(), line);
        }

        Handled readUnsupported(const QList<Unsupported>& features, const QString& line) const
        {
            for (const Unsupported& unsupported : features)
            {
                if (!unsupported.pattern.match(line).hasMatch())
                    continue;
                return lineError(parsing::unsupportedFeature(line, unsupported.feature,
                                                             unsupported.hint + QStringLiteral(". ")
                                                                 + kUnderstands));
            }
            return std::nullopt;
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

        int scope() const { return m_composites.isEmpty() ? -1 : m_composites.last().state; }

        int appendState(const QString& id, const QString& label, StateKind kind = StateKind::Normal)
        {
            State state;
            state.id = id;
            state.label = label;
            state.kind = kind;
            state.parent = scope();
            state.line = m_lineNumber;
            m_diagram.states.append(state);
            return int(m_diagram.states.size()) - 1;
        }

        int resolve(const QString& token, StateKind pseudoKind)
        {
            if (token == kInitialOrFinal)
                return pseudoState(pseudoKind);
            const QString id = unquoted(token);
            if (const int existing = indexOf(id); existing >= 0)
                return existing;
            return appendState(id, labelText(id));
        }

        int pseudoState(StateKind kind)
        {
            for (qsizetype index = 0; index < m_diagram.states.size(); ++index)
            {
                const State& state = m_diagram.states.at(index);
                if (state.kind == kind && state.parent == scope())
                    return int(index);
            }
            return appendState(kInitialOrFinal, QString(), kind);
        }

        int indexOf(const QString& id) const
        {
            for (qsizetype index = 0; index < m_diagram.states.size(); ++index)
            {
                const State& state = m_diagram.states.at(index);
                if (state.kind == StateKind::Normal && state.id == id)
                    return int(index);
            }
            return -1;
        }

        bool hasNote(int state, StateNoteSide side) const
        {
            for (const StateNote& note : m_diagram.notes)
            {
                if (note.state == state && note.side == side)
                    return true;
            }
            return m_openNote && m_openNote->state == state && m_openNote->side == side;
        }

        Error finish()
        {
            if (m_openNote)
                return lineErrorAt(m_lastContentLine,
                                   QStringLiteral("the note started at line %1 has no closing "
                                                  "\"end note\". Add \"end note\" after the text.")
                                       .arg(m_openNote->line));
            if (const auto unclosed = m_skippedBlocks.unclosed())
                return lineErrorAt(
                    unclosed->first,
                    QStringLiteral("\"%1\" block is not closed").arg(unclosed->second));
            if (!m_composites.isEmpty())
            {
                const Composite& open = m_composites.last();
                return lineErrorAt(
                    m_lastContentLine,
                    QStringLiteral("the composite state \"%1\" opened at line %2 is not closed "
                                   "with \"}\". Add \"}\" where the composite state ends.")
                        .arg(m_diagram.states.at(open.state).id)
                        .arg(open.line));
            }
            if (m_diagram.states.isEmpty())
                return QStringLiteral("The diagram has no states");
            return std::nullopt;
        }

        void warn(const QString& text)
        {
            m_diagram.warnings.append(parsing::lineErrorText(m_lineNumber, text));
        }

        Error lineError(const QString& text) const { return lineErrorAt(m_lineNumber, text); }

        static Error lineErrorAt(int line, const QString& text)
        {
            return parsing::lineErrorText(line, text);
        }

        StateDiagram m_diagram;
        QList<Composite> m_composites;
        QSet<int> m_declared;
        int m_lineNumber = 0;
        int m_lastContentLine = 0;
        std::optional<OpenNote> m_openNote;
        parsing::SkippedBlocks m_skippedBlocks;
    };
}

Result<StateDiagram> StateParser::parse(const QString& text) { return StateReader().read(text); }
}
