#pragma once

#include <QList>
#include <QString>

namespace diagram
{
struct EmbeddedDiagram
{
    qsizetype start = 0;
    qsizetype end = 0;
    int line = 0;
    QString text;
    bool closed = false;
};

struct LocatedError
{
    int line = 0;
    QString message;
};

class EmbeddedDiagrams
{
public:
    static QList<EmbeddedDiagram> find(const QString& text);
    static QList<int> strayMarkers(const QString& text);
    static LocatedError locate(const EmbeddedDiagram& diagram, const QString& error);
};
}
