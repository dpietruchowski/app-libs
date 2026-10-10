#include "parsersupport.h"

namespace diagram::parsing
{
namespace
{
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
}

QRegularExpression::PatternOptions patternOptions()
{
    return QRegularExpression::CaseInsensitiveOption
        | QRegularExpression::UseUnicodePropertiesOption;
}

QRegularExpression statement(const QString& source)
{
    return QRegularExpression(QStringLiteral("^") + source + QStringLiteral("$"), patternOptions());
}

QRegularExpression fragment(const QString& source)
{
    return QRegularExpression(source, patternOptions());
}

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

int braceBalance(const QString& line)
{
    return int(line.count(QLatin1Char('{'))) - int(line.count(QLatin1Char('}')));
}

QString tooLongMessage()
{
    return QStringLiteral("The diagram is longer than %1 characters").arg(kMaxLength);
}

QString lineErrorText(int line, const QString& text)
{
    return QStringLiteral("Line %1: %2").arg(line).arg(text);
}

QString unsupportedFeature(const QString& line, const QString& feature, const QString& rewrite)
{
    return QStringLiteral("\"%1\" is valid PlantUML, but this app does not support %2. %3.")
        .arg(line, feature, rewrite);
}

bool isDiagramMarker(const QString& line)
{
    static const QRegularExpression marker = statement(QStringLiteral(R"(@(start|end)uml\b.*)"));
    return marker.match(line).hasMatch();
}

std::optional<QString> SkippedBlocks::start(const QString& line, int lineNumber)
{
    for (const SkippedBlock& block : skippedBlocks())
    {
        const auto match = block.start.match(line);
        if (!match.hasMatch())
            continue;
        Open open { &block, lineNumber, braceBalance(line) };
        const bool closed = block.countsBraces
            ? open.depth <= 0
            : block.end.match(line, match.capturedEnd()).hasMatch();
        if (!closed)
            m_open = open;
        return block.warns ? QStringLiteral("\"%1\" block ignored").arg(block.name) : QString();
    }
    return std::nullopt;
}

void SkippedBlocks::continueWith(const QString& line)
{
    if (m_open->block->countsBraces)
    {
        m_open->depth += braceBalance(line);
        if (m_open->depth <= 0)
            m_open.reset();
        return;
    }
    if (m_open->block->end.match(line).hasMatch())
        m_open.reset();
}

std::optional<std::pair<int, QString>> SkippedBlocks::unclosed() const
{
    if (!m_open)
        return std::nullopt;
    return std::make_pair(m_open->startLine, m_open->block->name);
}
}
