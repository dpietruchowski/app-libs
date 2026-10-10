#include "diagramparser.h"

#include "diagram/activityparser.h"
#include "diagram/parsersupport.h"
#include "diagram/sequenceparser.h"

#include <QRegularExpression>
#include <QStringList>

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
        return parsing::isDiagramMarker(line) || title.match(line).hasMatch()
            || directive.match(line).hasMatch();
    }

    bool startsAnActivity(const QString& line)
    {
        static const QRegularExpression starts = fragment(QStringLiteral(
            R"(^(start$|:|#[^\s:]*:|if\s*\(|while\s*\(|repeat(\s*$|\s+(while\b|(#[^\s:]*)?:))|)"
            R"(fork\s*$|partition\b.*\{\s*$|switch\s*\(|\|(?!\|)|\(\*\)))"));
        return starts.match(line).hasMatch();
    }

    bool isActivityText(const QString& text)
    {
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
            if (isSkippableBeforeTheBody(line))
                continue;
            return startsAnActivity(line);
        }
        return false;
    }
}

Result<Diagram> DiagramParser::parse(const QString& text)
{
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
