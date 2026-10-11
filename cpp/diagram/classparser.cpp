#include "classparser.h"

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

    const QString kName = QStringLiteral(R"re((?:"[^"]+"|\w+(?:<[^<>\n]*>)?))re");
    const QString kKeyword = QStringLiteral(R"((?:abstract\s+class|class|struct|enum|interface))");
    const QString kArrow = QStringLiteral(R"re((<\||\*|o|<)?(-+|\.+)(\|>|\*|o|>)?)re");

    const QString kUnderstands = QStringLiteral(
        "A class diagram here understands: class Name, abstract class Name, struct Name, enum "
        "Name and interface Name (add { to hold members, one per line, a -- line starts a new "
        "compartment, close it with }), class \"Long name\" as Alias, relations A <|-- B, "
        "B --|> A, A <|.. B, A *-- B, A o-- B, A --> B, A ..> B, A -- B and A .. B with an "
        "optional \"1\" multiplicity at either end and an optional : label, note left of/right "
        "of Name : text, title");

    QString unquoted(const QString& token)
    {
        if (token.size() >= 2 && token.startsWith(QLatin1Char('"'))
            && token.endsWith(QLatin1Char('"')))
            return token.mid(1, token.size() - 2).trimmed();
        return token;
    }

    bool isQuoted(const QString& token) { return token.startsWith(QLatin1Char('"')); }

    QString idOf(const QString& token)
    {
        static const QRegularExpression generic(QStringLiteral(R"(^(\w+)<.*>$)"));
        if (isQuoted(token))
            return unquoted(token);
        const auto match = generic.match(token);
        return match.hasMatch() ? match.captured(1) : token;
    }

    ClassLinkEnd leftEndOf(const QString& token)
    {
        if (token == QStringLiteral("<|"))
            return ClassLinkEnd::Triangle;
        if (token == QStringLiteral("*"))
            return ClassLinkEnd::FilledDiamond;
        if (token == QStringLiteral("o"))
            return ClassLinkEnd::HollowDiamond;
        if (token == QStringLiteral("<"))
            return ClassLinkEnd::Arrow;
        return ClassLinkEnd::None;
    }

    ClassLinkEnd rightEndOf(const QString& token)
    {
        if (token == QStringLiteral("|>"))
            return ClassLinkEnd::Triangle;
        if (token == QStringLiteral("*"))
            return ClassLinkEnd::FilledDiamond;
        if (token == QStringLiteral("o"))
            return ClassLinkEnd::HollowDiamond;
        if (token == QStringLiteral(">"))
            return ClassLinkEnd::Arrow;
        return ClassLinkEnd::None;
    }

    ClassKind kindOf(const QString& keyword)
    {
        const QString lower = keyword.toLower();
        if (lower.startsWith(QStringLiteral("abstract")))
            return ClassKind::Abstract;
        if (lower == QStringLiteral("struct"))
            return ClassKind::Struct;
        if (lower == QStringLiteral("enum"))
            return ClassKind::Enum;
        if (lower == QStringLiteral("interface"))
            return ClassKind::Interface;
        return ClassKind::Class;
    }

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
              QStringLiteral("Remove the line; the app colours every class itself") },
            { fragment(QStringLiteral(R"(^(hide|show)\b)")),
              QStringLiteral("\"hide\" and \"show\""), QStringLiteral("Remove the line") },
            { fragment(QStringLiteral(R"(^(left\s+to\s+right|top\s+to\s+bottom)\s+direction$)")),
              QStringLiteral("layout directions"),
              QStringLiteral("Remove the line; the classes are placed top to bottom") },
        };
        return directives;
    }

    const QList<Unsupported>& unsupportedForms()
    {
        static const QList<Unsupported> forms {
            { fragment(QStringLiteral(R"(^(package|namespace)\b)")),
              QStringLiteral("\"package\" and \"namespace\" blocks"),
              QStringLiteral("Draw the classes without a block around them") },
            { fragment(QStringLiteral(R"(^together\b)")), QStringLiteral("\"together\" blocks"),
              QStringLiteral("Remove the block and keep the classes") },
            { fragment(QStringLiteral(R"(^[^:]*<<)")),
              QStringLiteral("stereotypes such as <<entity>>"),
              QStringLiteral("Draw the class without the stereotype") },
            { fragment(QStringLiteral(R"(^[^:]*(?:\(\)\s*[-.]{2}|[-.]{2}\s*\(\)))")),
              QStringLiteral("interface lollipops such as ()--"),
              QStringLiteral("Declare the interface with interface Name and link it with <|.. or "
                             "-->") },
            { fragment(QStringLiteral(R"(^[^:]*\(\s*[\w"]+\s*,\s*[\w"]+\s*\))")),
              QStringLiteral("association classes such as (A, B) .. C"),
              QStringLiteral("Link the class to both sides with plain relations instead") },
            { fragment(
                  QStringLiteral(R"re(\s[^\s"]*[-.](up|down|left|right|u|d|l|r)[-.][^\s"]*\s)re")),
              QStringLiteral("direction hints in arrows"),
              QStringLiteral("Write the arrow without the direction, for example A --> B") },
            { fragment(QStringLiteral(R"(\s[^\s"]*[-.]\[[^\s"]*\][^\s"]*\s)")),
              QStringLiteral("coloured or styled arrows"),
              QStringLiteral("Write the arrow without the brackets") },
            { fragment(QStringLiteral(R"(^[^:]*\s#\w+)")), QStringLiteral("colours"),
              QStringLiteral("Remove the colour; the app colours every class itself") },
            { fragment(QStringLiteral(R"(^[hr]?note\s+(top|bottom)\b)")),
              QStringLiteral("notes above or below a class"),
              QStringLiteral("Use note left of Name : text or note right of Name : text") },
            { fragment(QStringLiteral(R"(^(floating\s+note\b|[hr]?note\s+on\s+link\b))")),
              QStringLiteral("floating notes and notes on relations"),
              QStringLiteral("Attach the note to a class: note left of Name : text") },
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
            { fragment(QStringLiteral(R"(^(abstract\s+class|class|struct|enum|interface)\b)")),
              QStringLiteral("class"),
              QStringLiteral("class Name, abstract class Name, struct Name, enum Name or "
                             "interface Name, with class \"Long name\" as Alias, and { to open a "
                             "body") },
            { fragment(QStringLiteral(R"(^note\b)")), QStringLiteral("note"),
              QStringLiteral("note left of Name : text or note right of Name : text") },
            { fragment(QStringLiteral(R"(^title\b)")), QStringLiteral("title"),
              QStringLiteral("title Text") },
        };
        return forms;
    }

    struct OpenBody
    {
        int type = -1;
        int line = 0;
        QList<QStringList> compartments { QStringList {} };
        bool isSeparated = false;
    };

    struct OpenNote
    {
        int line = 0;
        int type = -1;
        ClassNoteSide side = ClassNoteSide::Right;
        QStringList lines;
    };

    class ClassReader
    {
    public:
        Result<ClassDiagram> read(const QString& text)
        {
            if (text.size() > parsing::kMaxLength)
                return Result<ClassDiagram>::failure(parsing::tooLongMessage());
            const QStringList lines = parsing::normalized(text).split(QLatin1Char('\n'));
            for (qsizetype index = 0; index < lines.size(); ++index)
            {
                m_lineNumber = int(index) + 1;
                const QString line = lines.at(index).trimmed();
                if (!line.isEmpty())
                    m_lastContentLine = m_lineNumber;
                if (const Error error = readLine(line))
                    return Result<ClassDiagram>::failure(*error);
                if (elementCount() > kMaxElements)
                    return Result<ClassDiagram>::failure(*lineError(
                        QStringLiteral("the diagram has more than %1 elements").arg(kMaxElements)));
            }
            if (const Error error = finish())
                return Result<ClassDiagram>::failure(*error);
            return Result<ClassDiagram>::success(m_diagram);
        }

    private:
        qsizetype elementCount() const
        {
            return m_diagram.types.size() + m_diagram.relations.size() + m_diagram.notes.size()
                + m_memberCount;
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
            if (m_body && !line.startsWith(QStringLiteral("/'")))
                return readBodyLine(line);
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

        Error readBodyLine(const QString& line)
        {
            static const QRegularExpression close = statement(QStringLiteral(R"(\})"));
            static const QRegularExpression separator = statement(QStringLiteral(R"(--+)"));
            static const QRegularExpression modifier
                = fragment(QStringLiteral(R"(\{(static|abstract|field|method|classifier)\})"));

            if (close.match(line).hasMatch())
            {
                closeBody();
                return std::nullopt;
            }
            if (separator.match(line).hasMatch())
            {
                m_body->isSeparated = true;
                m_body->compartments.append(QStringList {});
                return std::nullopt;
            }
            if (modifier.match(line).hasMatch())
                return lineError(parsing::unsupportedFeature(
                    line, QStringLiteral("member modifiers such as {static} and {abstract}"),
                    QStringLiteral("Remove the modifier. ") + kUnderstands));
            m_body->compartments.last().append(line);
            ++m_memberCount;
            return std::nullopt;
        }

        void closeBody()
        {
            ClassType& type = m_diagram.types[m_body->type];
            QList<QStringList> compartments = m_body->compartments;
            if (!m_body->isSeparated && type.kind != ClassKind::Enum)
                compartments = fieldsThenMethods(compartments.first());
            for (const QStringList& compartment : std::as_const(compartments))
            {
                if (!compartment.isEmpty())
                    type.compartments.append(compartment);
            }
            m_body.reset();
        }

        static QList<QStringList> fieldsThenMethods(const QStringList& members)
        {
            QStringList fields;
            QStringList methods;
            for (const QString& member : members)
                (member.contains(QLatin1Char('(')) ? methods : fields).append(member);
            return { fields, methods };
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
            for (const auto reader :
                 { &ClassReader::readClose, &ClassReader::readDeclaration,
                   &ClassReader::readRelation, &ClassReader::readNote, &ClassReader::readIgnored,
                   &ClassReader::readUnsupportedForm })
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
            return lineError(QStringLiteral("\"}\" without an open class body"));
        }

        Handled readDeclaration(const QString& line)
        {
            static const QRegularExpression declaration
                = statement(QStringLiteral(R"((%1)\s+(%2)(?:\s+as\s+(%2))?\s*(?:(\{)\s*(\})?)?)")
                                .arg(kKeyword, kName));
            const auto match = declaration.match(line);
            if (!match.hasMatch())
                return std::nullopt;

            const QString keyword = match.captured(1);
            const QString name = match.captured(2);
            const QString alias = match.captured(3);
            QString id = idOf(name);
            QString label = unquoted(name);
            if (!alias.isEmpty())
            {
                id = isQuoted(alias) ? idOf(name) : alias;
                label = isQuoted(alias) ? unquoted(alias) : unquoted(name);
            }
            label = labelText(label);
            const bool opens = !match.captured(4).isEmpty();
            const bool closes = !match.captured(5).isEmpty();

            int index = indexOf(id);
            if (index >= 0 && m_declared.contains(index))
                return lineError(QStringLiteral("the class \"%1\" is already declared at line %2. "
                                                "Give each class its own name.")
                                     .arg(id)
                                     .arg(m_diagram.types.at(index).line));
            if (index < 0)
            {
                index = appendType(id, label);
            }
            else
            {
                m_diagram.types[index].label = label;
                m_diagram.types[index].line = m_lineNumber;
            }
            m_diagram.types[index].kind = kindOf(keyword);
            m_declared.insert(index);
            if (opens)
            {
                OpenBody body;
                body.type = index;
                body.line = m_lineNumber;
                m_body = body;
                if (closes)
                    closeBody();
            }
            return Error {};
        }

        Handled readRelation(const QString& line)
        {
            static const QRegularExpression relation = statement(
                QStringLiteral(
                    R"re((%1)\s*(?:"([^"]*)"\s*)?%2\s*(?:"([^"]*)"\s*)?(%1)\s*(?::(.*))?)re")
                    .arg(kName, kArrow));
            const auto match = relation.match(line);
            if (!match.hasMatch())
                return std::nullopt;

            ClassRelation parsed;
            parsed.from = resolve(match.captured(1));
            parsed.fromMultiplicity = match.captured(2).trimmed();
            parsed.fromEnd = leftEndOf(match.captured(3));
            parsed.line = match.captured(4).startsWith(QLatin1Char('-')) ? ClassLinkLine::Solid
                                                                         : ClassLinkLine::Dashed;
            parsed.toEnd = rightEndOf(match.captured(5));
            parsed.toMultiplicity = match.captured(6).trimmed();
            parsed.to = resolve(match.captured(7));
            parsed.label = labelText(match.captured(8));
            parsed.sourceLine = m_lineNumber;
            m_diagram.relations.append(parsed);
            return Error {};
        }

        Handled readNote(const QString& line)
        {
            static const QRegularExpression note = statement(
                QStringLiteral(R"([hr]?note\s+(left|right)\s+of\s+(%1)\s*(?::(.*))?)").arg(kName));
            const auto match = note.match(line);
            if (!match.hasMatch())
                return std::nullopt;

            const QString id = idOf(match.captured(2));
            const int type = indexOf(id);
            if (type < 0)
                return lineError(QStringLiteral("unknown class \"%1\" in the note. Declare it "
                                                "before the note.")
                                     .arg(id));
            const bool left
                = match.captured(1).compare(QStringLiteral("left"), Qt::CaseInsensitive) == 0;
            const ClassNoteSide side = left ? ClassNoteSide::Left : ClassNoteSide::Right;
            if (hasNote(type, side))
                return lineError(
                    QStringLiteral("the class \"%1\" already has a note on the %2. "
                                   "Merge the two notes into one.")
                        .arg(id, left ? QStringLiteral("left") : QStringLiteral("right")));
            if (match.hasCaptured(3))
            {
                m_diagram.notes.append({ type, side, labelText(match.captured(3)) });
                return Error {};
            }
            m_openNote = OpenNote { m_lineNumber, type, side, {} };
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
            m_diagram.notes.append({ finished.type, finished.side,
                                     labelText(finished.lines.join(QLatin1Char('\n'))) });
            return std::nullopt;
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
            static const QRegularExpression malformedRelation = statement(
                QStringLiteral(R"re(%1\s*(?:"[^"]*"\s*)?(?=[<*ox]|[^\s\w"])\S*[-.=]\S*\s+)re"
                               R"re((?:"[^"]*"\s*)?%1\s*(?::.*)?)re")
                    .arg(kName));
            if (malformedRelation.match(line).hasMatch())
                return QStringLiteral("\"%1\" is not a valid relation. Write it as: A <|-- B, "
                                      "B --|> A, A <|.. B, A *-- B, A o-- B, A --> B, A ..> B, "
                                      "A -- B or A .. B.")
                    .arg(line);
            for (const Form& form : knownForms())
            {
                if (form.pattern.match(line).hasMatch())
                    return QStringLiteral(
                               "\"%1\" is not a valid \"%2\" statement. Write it as: %3.")
                        .arg(line, form.keyword, form.form);
            }
            return QStringLiteral("unknown statement \"%1\". %2.").arg(line, kUnderstands);
        }

        int appendType(const QString& id, const QString& label)
        {
            ClassType type;
            type.id = id;
            type.label = label;
            type.line = m_lineNumber;
            m_diagram.types.append(type);
            return int(m_diagram.types.size()) - 1;
        }

        int resolve(const QString& token)
        {
            const QString id = idOf(token);
            if (const int existing = indexOf(id); existing >= 0)
                return existing;
            return appendType(id, labelText(unquoted(token)));
        }

        int indexOf(const QString& id) const
        {
            for (qsizetype index = 0; index < m_diagram.types.size(); ++index)
            {
                if (m_diagram.types.at(index).id == id)
                    return int(index);
            }
            return -1;
        }

        bool hasNote(int type, ClassNoteSide side) const
        {
            for (const ClassNote& note : m_diagram.notes)
            {
                if (note.type == type && note.side == side)
                    return true;
            }
            return m_openNote && m_openNote->type == type && m_openNote->side == side;
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
            if (m_body)
                return lineErrorAt(
                    m_lastContentLine,
                    QStringLiteral("the body of the class \"%1\" opened at line %2 is not closed "
                                   "with \"}\". Add \"}\" where the body ends.")
                        .arg(m_diagram.types.at(m_body->type).id)
                        .arg(m_body->line));
            if (m_diagram.types.isEmpty())
                return QStringLiteral("The diagram has no classes");
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

        ClassDiagram m_diagram;
        std::optional<OpenBody> m_body;
        QSet<int> m_declared;
        qsizetype m_memberCount = 0;
        int m_lineNumber = 0;
        int m_lastContentLine = 0;
        std::optional<OpenNote> m_openNote;
        parsing::SkippedBlocks m_skippedBlocks;
    };
}

Result<ClassDiagram> ClassParser::parse(const QString& text) { return ClassReader().read(text); }
}
