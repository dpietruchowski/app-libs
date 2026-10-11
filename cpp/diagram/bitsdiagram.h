#pragma once

#include <QList>
#include <QString>
#include <QStringList>

namespace diagram
{
enum class BitsMode
{
    Bits,
    Bytes
};

enum class BitsSpanKind
{
    Field,
    Padding
};

struct BitsSpan
{
    int low = 0;
    int high = 0;
    BitsSpanKind kind = BitsSpanKind::Field;
    QString label;
    QString description;
    int line = 0;
};

struct BitsDiagram
{
    QString title;
    BitsMode mode = BitsMode::Bits;
    int width = 0;
    QList<BitsSpan> spans;
    QStringList warnings;
};
}
