#include "embeddeddiagrams.h"

#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <optional>

namespace diagram
{
namespace
{
    struct MarkerPair
    {
        QString start;
        QString end;
    };

    const QList<MarkerPair>& markerPairs()
    {
        static const QList<MarkerPair> pairs {
            { QStringLiteral("@startuml"), QStringLiteral("@enduml") },
            { QStringLiteral("@startbits"), QStringLiteral("@endbits") },
        };
        return pairs;
    }

    bool opens(QStringView line, const MarkerPair& pair)
    {
        return line.startsWith(pair.start, Qt::CaseInsensitive);
    }

    bool closes(QStringView line, const QString& end)
    {
        return line.compare(end, Qt::CaseInsensitive) == 0;
    }

    std::optional<MarkerPair> openedBy(QStringView line)
    {
        for (const MarkerPair& pair : markerPairs())
        {
            if (opens(line, pair))
                return pair;
        }
        return std::nullopt;
    }

    bool closesAny(QStringView line)
    {
        return std::any_of(markerPairs().cbegin(), markerPairs().cend(),
                           [line](const MarkerPair& pair) { return closes(line, pair.end); });
    }

    std::optional<MarkerPair> mentionedIn(QStringView line)
    {
        for (const MarkerPair& pair : markerPairs())
        {
            if (line.contains(pair.start, Qt::CaseInsensitive)
                || line.contains(pair.end, Qt::CaseInsensitive))
                return pair;
        }
        return std::nullopt;
    }

    QString bodyOf(QString raw)
    {
        raw.remove(QLatin1Char('\r'));
        raw.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
        raw.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
        raw.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
        raw.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
        return raw;
    }
}

QList<StrayMarker> EmbeddedDiagrams::findStrayMarkers(const QString& text)
{
    QList<StrayMarker> strays;
    const QStringList all = text.split(QLatin1Char('\n'));
    for (int index = 0; index < all.size(); ++index)
    {
        const QStringView content = QStringView(all.at(index)).trimmed();
        const auto mentioned = mentionedIn(content);
        if (mentioned && !openedBy(content) && !closesAny(content))
            strays.append({ index, mentioned->start, mentioned->end });
    }
    return strays;
}

QList<int> EmbeddedDiagrams::strayMarkers(const QString& text)
{
    QList<int> lines;
    for (const StrayMarker& stray : findStrayMarkers(text))
        lines.append(stray.line);
    return lines;
}

QList<EmbeddedDiagram> EmbeddedDiagrams::find(const QString& text)
{
    QList<EmbeddedDiagram> diagrams;
    std::optional<EmbeddedDiagram> open;
    qsizetype bodyStart = 0;
    qsizetype position = 0;
    int line = 0;
    while (position <= text.size())
    {
        qsizetype lineEnd = text.indexOf(QLatin1Char('\n'), position);
        if (lineEnd < 0)
            lineEnd = text.size();
        const QStringView content = QStringView(text).mid(position, lineEnd - position).trimmed();
        if (const auto pair = open ? std::nullopt : openedBy(content))
        {
            open = EmbeddedDiagram { position, lineEnd,     line,     QString(),
                                     false,    pair->start, pair->end };
            bodyStart = std::min(lineEnd + 1, text.size());
        }
        else if (open && closes(content, open->endMarker))
        {
            open->end = lineEnd;
            open->text
                = bodyOf(text.mid(bodyStart, std::max<qsizetype>(0, position - 1 - bodyStart)));
            open->closed = true;
            diagrams.append(*open);
            open.reset();
        }
        position = lineEnd + 1;
        ++line;
    }
    if (open)
    {
        open->end = text.size();
        open->text = bodyOf(text.mid(bodyStart));
        diagrams.append(*open);
    }
    return diagrams;
}

LocatedError EmbeddedDiagrams::locate(const EmbeddedDiagram& diagram, const QString& error)
{
    static const QRegularExpression numbered(QStringLiteral(R"(^Line (\d+): (.*)$)"),
                                             QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch match = numbered.match(error);
    if (!match.hasMatch())
        return { diagram.line, error };
    return { diagram.line + match.captured(1).toInt(), match.captured(2) };
}
}
