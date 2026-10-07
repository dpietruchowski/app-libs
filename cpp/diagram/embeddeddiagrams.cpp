#include "embeddeddiagrams.h"

#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <optional>

namespace diagram
{
namespace
{
    const QString kStart = QStringLiteral("@startuml");
    const QString kEnd = QStringLiteral("@enduml");

    bool opens(QStringView line) { return line.startsWith(kStart, Qt::CaseInsensitive); }

    bool closes(QStringView line) { return line.compare(kEnd, Qt::CaseInsensitive) == 0; }

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

QList<int> EmbeddedDiagrams::strayMarkers(const QString& text)
{
    QList<int> lines;
    const QStringList all = text.split(QLatin1Char('\n'));
    for (int index = 0; index < all.size(); ++index)
    {
        const QStringView content = QStringView(all.at(index)).trimmed();
        const bool mentions = content.contains(kStart, Qt::CaseInsensitive)
            || content.contains(kEnd, Qt::CaseInsensitive);
        if (mentions && !opens(content) && !closes(content))
            lines.append(index);
    }
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
        if (!open && opens(content))
        {
            open = EmbeddedDiagram { position, lineEnd, line, QString(), false };
            bodyStart = std::min(lineEnd + 1, text.size());
        }
        else if (open && closes(content))
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
