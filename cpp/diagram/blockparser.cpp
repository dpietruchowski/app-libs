#include "blockparser.h"

#include "diagram/parsersupport.h"

#include <QRegularExpression>

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

    const QString kName = QStringLiteral(R"((?:"[^"]+"|\w+))");

    QString unquoted(const QString& token)
    {
        if (token.size() >= 2 && token.startsWith(QLatin1Char('"'))
            && token.endsWith(QLatin1Char('"')))
            return token.mid(1, token.size() - 2).trimmed();
        return token;
    }

    bool isQuoted(const QString& token) { return token.startsWith(QLatin1Char('"')); }

    BlockKind kindOf(const QString& keyword)
    {
        if (keyword.compare(QStringLiteral("node"), Qt::CaseInsensitive) == 0)
            return BlockKind::Node;
        if (keyword.compare(QStringLiteral("component"), Qt::CaseInsensitive) == 0)
            return BlockKind::Component;
        if (keyword.compare(QStringLiteral("database"), Qt::CaseInsensitive) == 0)
            return BlockKind::Database;
        return BlockKind::Rectangle;
    }

    struct Unsupported
    {
        QRegularExpression pattern;
        QString feature;
        QString rewrite;
    };

    const QList<Unsupported>& unsupportedFeatures()
    {
        static const QList<Unsupported> features {
            { fragment(QStringLiteral(
                  R"(^(package|folder|frame|cloud|storage|artifact|queue|stack|file|card|agent|)"
                  R"(label|hexagon|person|actor|usecase|boundary|control|entity|collections|)"
                  R"(port|portin|portout)\s+("|\w|\{))")),
              QStringLiteral("\"%1\" elements"),
              QStringLiteral("Draw it as a rectangle, node, component or database, and group "
                             "blocks with a rectangle and { }") },
            { fragment(QStringLiteral(R"(^(interface\b|\(\)))")), QStringLiteral("interfaces"),
              QStringLiteral("Draw the interface as a small rectangle and link to it") },
            { fragment(QStringLiteral(R"(^(component\s+)?\[[^\]]+\])")),
              QStringLiteral("the [Component] short syntax"),
              QStringLiteral("Declare the block as: component \"Name\" as Alias") },
            { fragment(QStringLiteral(R"(^(left\s+to\s+right|top\s+to\s+bottom)\s+direction$)")),
              QStringLiteral("layout directions"),
              QStringLiteral("Remove the line; the blocks are placed top to bottom") },
            { fragment(QStringLiteral(R"(^[hr]?note\s+(top|bottom)\b)")),
              QStringLiteral("notes above or below a block"),
              QStringLiteral("Use note left of Alias : text or note right of Alias : text") },
            { fragment(QStringLiteral(R"(^(floating\s+note\b|[hr]?note\s+on\s+link\b))")),
              QStringLiteral("floating notes and notes on links"),
              QStringLiteral("Attach the note to a block: note left of Alias : text") },
            { fragment(QStringLiteral(R"re(^(?:"[^"]+"|\w+)\s+\S*[-.]\S*\s+(?:"[^"]+"|\w+))re")),
              QStringLiteral("this kind of arrow"),
              QStringLiteral("Write the link as A -- B, A --> B, A <-- B or A .. B") },
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
            { fragment(QStringLiteral(R"(^(rectangle|node|component|database)\b)")),
              QStringLiteral("block"),
              QStringLiteral("rectangle \"Label\" as Alias, or rectangle \"Label\" { to open a "
                             "container") },
            { fragment(QStringLiteral(R"(^note\b)")), QStringLiteral("note"),
              QStringLiteral("note left of Alias : text or note right of Alias : text") },
            { fragment(QStringLiteral(R"(^title\b)")), QStringLiteral("title"),
              QStringLiteral("title Text") },
        };
        return forms;
    }

    struct Container
    {
        int block = -1;
        int line = 0;
    };

    struct OpenNote
    {
        int line = 0;
        int block = -1;
        BlockNoteSide side = BlockNoteSide::Right;
        QStringList lines;
    };

    class BlockReader
    {
    public:
        Result<BlockDiagram> read(const QString& text)
        {
            if (text.size() > parsing::kMaxLength)
                return Result<BlockDiagram>::failure(parsing::tooLongMessage());
            const QStringList lines = parsing::normalized(text).split(QLatin1Char('\n'));
            for (qsizetype index = 0; index < lines.size(); ++index)
            {
                m_lineNumber = int(index) + 1;
                const QString line = lines.at(index).trimmed();
                if (!line.isEmpty())
                    m_lastContentLine = m_lineNumber;
                if (const Error error = readLine(line))
                    return Result<BlockDiagram>::failure(*error);
                if (elementCount() > kMaxElements)
                    return Result<BlockDiagram>::failure(*lineError(
                        QStringLiteral("the diagram has more than %1 elements").arg(kMaxElements)));
            }
            if (const Error error = finish())
                return Result<BlockDiagram>::failure(*error);
            return Result<BlockDiagram>::success(m_diagram);
        }

    private:
        qsizetype elementCount() const
        {
            return m_diagram.blocks.size() + m_diagram.links.size() + m_diagram.notes.size();
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
            for (const auto reader : { &BlockReader::readClose, &BlockReader::readDeclaration,
                                       &BlockReader::readLink, &BlockReader::readNote,
                                       &BlockReader::readIgnored, &BlockReader::readUnsupported })
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
            if (m_containers.isEmpty())
                return lineError(QStringLiteral("\"}\" without an open container"));
            m_containers.removeLast();
            return Error {};
        }

        Handled readDeclaration(const QString& line)
        {
            static const QRegularExpression declaration = statement(
                QStringLiteral(R"((rectangle|node|component|database)\s+(%1)(?:\s+as\s+(%1))?)"
                               R"((\s+<<[^>]*>>)?(\s+#\S+)?\s*(\{)?)")
                    .arg(kName));
            const auto match = declaration.match(line);
            if (!match.hasMatch())
                return std::nullopt;

            const QString name = match.captured(2);
            const QString alias = match.captured(3);
            Block block;
            block.id = unquoted(name);
            block.label = unquoted(name);
            if (!alias.isEmpty())
            {
                block.id = isQuoted(alias) ? unquoted(name) : alias;
                block.label = isQuoted(alias) ? unquoted(alias) : unquoted(name);
            }
            block.label = labelText(block.label);
            block.kind = kindOf(match.captured(1));
            block.isContainer = !match.captured(6).isEmpty();
            block.parent = m_containers.isEmpty() ? -1 : m_containers.last().block;
            block.line = m_lineNumber;
            if (!match.captured(4).isEmpty())
                warn(QStringLiteral("stereotype ignored"));
            if (!match.captured(5).isEmpty())
                warn(QStringLiteral("colour ignored"));

            if (const int existing = indexOf(block.id); existing >= 0)
                return lineError(QStringLiteral("the block \"%1\" is already declared at line %2. "
                                                "Give each block its own alias.")
                                     .arg(block.id)
                                     .arg(m_diagram.blocks.at(existing).line));
            m_diagram.blocks.append(block);
            if (block.isContainer)
                m_containers.append({ int(m_diagram.blocks.size()) - 1, m_lineNumber });
            return Error {};
        }

        Handled readLink(const QString& line)
        {
            static const QRegularExpression link = statement(
                QStringLiteral(R"((%1)\s*(<--|-->|--|\.\.)\s*(%1)\s*(?::(.*))?)").arg(kName));
            const auto match = link.match(line);
            if (!match.hasMatch())
                return std::nullopt;

            const int from = indexOf(unquoted(match.captured(1)));
            const int to = indexOf(unquoted(match.captured(3)));
            if (from < 0)
                return lineError(unknownBlock(unquoted(match.captured(1))));
            if (to < 0)
                return lineError(unknownBlock(unquoted(match.captured(3))));
            if (from == to)
                return lineError(QStringLiteral("the link \"%1\" joins a block to itself. Link two "
                                                "different blocks.")
                                     .arg(line));

            const QString arrow = match.captured(2);
            BlockLink parsed;
            parsed.from = from;
            parsed.to = to;
            parsed.label = labelText(match.captured(4));
            parsed.line
                = arrow == QStringLiteral("..") ? BlockLinkLine::Dotted : BlockLinkLine::Solid;
            if (arrow == QStringLiteral("-->"))
                parsed.head = BlockLinkHead::ToTarget;
            else if (arrow == QStringLiteral("<--"))
                parsed.head = BlockLinkHead::ToSource;
            parsed.sourceLine = m_lineNumber;
            m_diagram.links.append(parsed);
            return Error {};
        }

        QString unknownBlock(const QString& id) const
        {
            return QStringLiteral("unknown block \"%1\" in the link. Declare it before the link, "
                                  "for example: rectangle \"%1\" as %1.")
                .arg(id);
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
            const int block = indexOf(id);
            if (block < 0)
                return lineError(QStringLiteral("unknown block \"%1\" in the note. Declare it "
                                                "before the note.")
                                     .arg(id));
            if (!match.captured(3).isEmpty())
                warn(QStringLiteral("colour ignored"));
            const bool left
                = match.captured(1).compare(QStringLiteral("left"), Qt::CaseInsensitive) == 0;
            const BlockNoteSide side = left ? BlockNoteSide::Left : BlockNoteSide::Right;
            if (hasNote(block, side))
                return lineError(
                    QStringLiteral("the block \"%1\" already has a note on the %2. "
                                   "Merge the two notes into one.")
                        .arg(id, left ? QStringLiteral("left") : QStringLiteral("right")));
            if (match.hasCaptured(4))
            {
                m_diagram.notes.append({ block, side, labelText(match.captured(4)) });
                return Error {};
            }
            m_openNote = OpenNote { m_lineNumber, block, side, {} };
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
            m_diagram.notes.append({ finished.block, finished.side,
                                     labelText(finished.lines.join(QLatin1Char('\n'))) });
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
            return Error {};
        }

        Handled readUnsupported(const QString& line)
        {
            for (const Unsupported& unsupported : unsupportedFeatures())
            {
                const auto match = unsupported.pattern.match(line);
                if (!match.hasMatch())
                    continue;
                const QString feature = unsupported.feature.contains(QStringLiteral("%1"))
                    ? unsupported.feature.arg(match.captured(1).toLower())
                    : unsupported.feature;
                return lineError(parsing::unsupportedFeature(line, feature, unsupported.rewrite));
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
            return QStringLiteral(
                       "unknown statement \"%1\". A block diagram here understands: "
                       "rectangle/node/component/database \"Label\" as Alias (add { to hold "
                       "other blocks, close it with }), A -- B, A --> B, A <-- B, A .. B with an "
                       "optional : label, note left of/right of Alias : text, title.")
                .arg(line);
        }

        int indexOf(const QString& id) const
        {
            for (qsizetype index = 0; index < m_diagram.blocks.size(); ++index)
            {
                if (m_diagram.blocks.at(index).id == id)
                    return int(index);
            }
            return -1;
        }

        bool hasNote(int block, BlockNoteSide side) const
        {
            for (const BlockNote& note : m_diagram.notes)
            {
                if (note.block == block && note.side == side)
                    return true;
            }
            return m_openNote && m_openNote->block == block && m_openNote->side == side;
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
            if (!m_containers.isEmpty())
            {
                const Container& open = m_containers.last();
                return lineErrorAt(
                    m_lastContentLine,
                    QStringLiteral("the container \"%1\" opened at line %2 is not closed with "
                                   "\"}\". Add \"}\" where the container ends.")
                        .arg(m_diagram.blocks.at(open.block).id)
                        .arg(open.line));
            }
            if (m_diagram.blocks.isEmpty())
                return QStringLiteral("The diagram has no blocks");
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

        BlockDiagram m_diagram;
        QList<Container> m_containers;
        int m_lineNumber = 0;
        int m_lastContentLine = 0;
        std::optional<OpenNote> m_openNote;
        parsing::SkippedBlocks m_skippedBlocks;
    };
}

Result<BlockDiagram> BlockParser::parse(const QString& text) { return BlockReader().read(text); }
}
