#pragma once

#include <QRegularExpression>
#include <QString>

#include <optional>
#include <utility>

namespace diagram::parsing
{
using Error = std::optional<QString>;

constexpr qsizetype kMaxLength = 20000;

QRegularExpression::PatternOptions patternOptions();
QRegularExpression statement(const QString& source);
QRegularExpression fragment(const QString& source);

QString labelText(const QString& raw);
QString normalized(QString text);
int braceBalance(const QString& line);

QString tooLongMessage();
QString lineErrorText(int line, const QString& text);
QString unsupportedFeature(const QString& line, const QString& feature, const QString& rewrite);

bool isDiagramMarker(const QString& line);

struct SkippedBlock
{
    QString name;
    QRegularExpression start;
    QRegularExpression end;
    bool countsBraces = false;
    bool warns = true;
};

class SkippedBlocks
{
public:
    std::optional<QString> start(const QString& line, int lineNumber);
    bool isOpen() const { return m_open.has_value(); }
    void continueWith(const QString& line);
    std::optional<std::pair<int, QString>> unclosed() const;

private:
    struct Open
    {
        const SkippedBlock* block = nullptr;
        int startLine = 0;
        int depth = 0;
    };

    std::optional<Open> m_open;
};
}
