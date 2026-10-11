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
    QString startMarker = QStringLiteral("@startuml");
    QString endMarker = QStringLiteral("@enduml");
};

struct LocatedError
{
    int line = 0;
    QString message;
};

struct StrayMarker
{
    int line = 0;
    QString startMarker;
    QString endMarker;
};

class EmbeddedDiagrams
{
public:
    static QList<EmbeddedDiagram> find(const QString& text);
    static QList<int> strayMarkers(const QString& text);
    static QList<StrayMarker> findStrayMarkers(const QString& text);
    static LocatedError locate(const EmbeddedDiagram& diagram, const QString& error);
};
}
