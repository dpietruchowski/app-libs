#include "diagramparser.h"

#include "diagram/activityparser.h"
#include "diagram/bitsparser.h"
#include "diagram/blockparser.h"
#include "diagram/classparser.h"
#include "diagram/parsersupport.h"
#include "diagram/sequenceparser.h"
#include "diagram/stateparser.h"

#include <QRegularExpression>
#include <QStringList>

#include <optional>

namespace diagram
{
namespace
{
    using parsing::fragment;
    using parsing::statement;

    bool isSkippableBeforeTheBody(const QString& line)
    {
        static const QRegularExpression title = statement(QStringLiteral(R"(title\s+(?![-<]).+)"));
        static const QRegularExpression directive = statement(QStringLiteral(
            R"(((skinparam|hide|show|scale|header|footer|caption|autonumber)\b.*|!.*))"));
        return parsing::isDiagramMarker(line) || parsing::isBitsMarker(line)
            || title.match(line).hasMatch() || directive.match(line).hasMatch();
    }

    bool startsABitsDiagram(const QString& line)
    {
        static const QRegularExpression starts
            = fragment(QStringLiteral(R"(^((bits|bytes)\s+\w+$|(field|pad)\s+\d))"));
        return starts.match(line).hasMatch();
    }

    bool startsAnActivity(const QString& line)
    {
        static const QRegularExpression starts = fragment(QStringLiteral(
            R"(^(start$|:|#[^\s:]*:|if\s*\(|while\s*\(|repeat(\s*$|\s+(while\b|(#[^\s:]*)?:))|)"
            R"(fork\s*$|partition\b.*\{\s*$|switch\s*\(|\|(?!\|)|\(\*\)))"));
        return starts.match(line).hasMatch();
    }

    bool startsABlockDiagram(const QString& line)
    {
        static const QRegularExpression starts = fragment(QStringLiteral(
            R"(^((rectangle|node|component|database|package|folder|frame|cloud|interface|storage|)"
            R"(artifact)\s+("|\w)|\[[\w"]))"));
        return starts.match(line).hasMatch();
    }

    bool startsAStateDiagram(const QString& line)
    {
        static const QRegularExpression starts
            = fragment(QStringLiteral(R"(^(\[\*\]|state\s+("|\w)))"));
        return starts.match(line).hasMatch();
    }

    bool startsAClassDiagram(const QString& line)
    {
        static const QRegularExpression starts
            = fragment(QStringLiteral(R"(^(abstract\s+class|class|struct|enum)\s+("|\w))"));
        return starts.match(line).hasMatch();
    }

    bool startsWithADatabase(const QString& line)
    {
        static const QRegularExpression starts = fragment(QStringLiteral(R"(^database\s+("|\w))"));
        return starts.match(line).hasMatch();
    }

    bool isSequenceOnly(const QString& line)
    {
        static const QRegularExpression construct = fragment(
            QStringLiteral(R"(^((participant|actor|boundary|control|entity|collections|queue)\s+\S)"
                           R"(|(activate|deactivate|destroy)\s+\S)"
                           R"(|(alt|loop|opt|par|group|break|critical|return)\b)"
                           R"(|==.*==$|note\s+(over|across)\b|ref\s+over\b))"));
        static const QRegularExpression arrow
            = QRegularExpression(QStringLiteral(R"((?<!-)->|<-(?!-))"));
        if (construct.match(line).hasMatch())
            return true;
        const QString beforeTheLabel = line.left(line.indexOf(QLatin1Char(':')));
        return arrow.match(beforeTheLabel).hasMatch();
    }

    QStringList bodyLines(const QString& text)
    {
        QStringList body;
        parsing::SkippedBlocks skipped;
        const QStringList lines = parsing::normalized(text).split(QLatin1Char('\n'));
        for (qsizetype index = 0; index < lines.size(); ++index)
        {
            const QString line = lines.at(index).trimmed();
            if (skipped.isOpen())
            {
                skipped.continueWith(line);
                continue;
            }
            if (line.isEmpty() || line.startsWith(QLatin1Char('\'')))
                continue;
            if (skipped.start(line, int(index) + 1))
                continue;
            body.append(line);
        }
        return body;
    }

    std::optional<QString> firstBodyLine(const QStringList& body)
    {
        for (const QString& line : body)
        {
            if (!isSkippableBeforeTheBody(line))
                return line;
        }
        return std::nullopt;
    }

    bool hasASequenceOnlyConstruct(const QStringList& body)
    {
        bool insideNote = false;
        for (const QString& line : body)
        {
            static const QRegularExpression openNote
                = fragment(QStringLiteral(R"(^r?note\s+(left|right)\s+of\s+[^:]+$)"));
            static const QRegularExpression closeNote = fragment(QStringLiteral(R"(^end\s*note$)"));
            if (insideNote)
            {
                insideNote = !closeNote.match(line).hasMatch();
                continue;
            }
            if (openNote.match(line).hasMatch())
            {
                insideNote = true;
                continue;
            }
            if (!isSkippableBeforeTheBody(line) && isSequenceOnly(line))
                return true;
        }
        return false;
    }

    bool isActivityText(const QString& text)
    {
        const auto line = firstBodyLine(bodyLines(text));
        return line && startsAnActivity(*line);
    }

    bool isStateText(const QString& text)
    {
        const auto line = firstBodyLine(bodyLines(text));
        return line && startsAStateDiagram(*line);
    }

    bool isClassText(const QString& text)
    {
        const auto line = firstBodyLine(bodyLines(text));
        return line && startsAClassDiagram(*line);
    }

    bool isBitsText(const QString& text)
    {
        const auto line = firstBodyLine(bodyLines(text));
        return line && startsABitsDiagram(*line);
    }

    bool isBlockText(const QString& text)
    {
        const QStringList body = bodyLines(text);
        const auto line = firstBodyLine(body);
        if (!line || !startsABlockDiagram(*line))
            return false;
        return !startsWithADatabase(*line) || !hasASequenceOnlyConstruct(body);
    }
}

Result<Diagram> DiagramParser::parse(const QString& text)
{
    if (isBitsText(text))
    {
        const auto bits = BitsParser::parse(text);
        if (bits.isFailure())
            return Result<Diagram>::failure(bits.error());
        return Result<Diagram>::success(bits.value());
    }
    if (isBlockText(text))
    {
        const auto blocks = BlockParser::parse(text);
        if (blocks.isFailure())
            return Result<Diagram>::failure(blocks.error());
        return Result<Diagram>::success(blocks.value());
    }
    if (isStateText(text))
    {
        const auto states = StateParser::parse(text);
        if (states.isFailure())
            return Result<Diagram>::failure(states.error());
        return Result<Diagram>::success(states.value());
    }
    if (isClassText(text))
    {
        const auto classes = ClassParser::parse(text);
        if (classes.isFailure())
            return Result<Diagram>::failure(classes.error());
        return Result<Diagram>::success(classes.value());
    }
    if (isActivityText(text))
    {
        const auto activity = ActivityParser::parse(text);
        if (activity.isFailure())
            return Result<Diagram>::failure(activity.error());
        return Result<Diagram>::success(activity.value());
    }
    const auto sequence = SequenceParser::parse(text);
    if (sequence.isFailure())
        return Result<Diagram>::failure(sequence.error());
    return Result<Diagram>::success(sequence.value());
}
}
