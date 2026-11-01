#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>

#include "fixturefile.h"

namespace chat::test
{

inline QList<QJsonObject> recordedRequests(const char* fixture)
{
    QList<QJsonObject> entries;
    for (const QByteArray& line : readFixture(fixture).split('\n'))
    {
        if (!line.trimmed().isEmpty())
            entries.append(QJsonDocument::fromJson(line).object());
    }
    return entries;
}

inline QByteArray wireFormat(const QJsonObject& entry)
{
    QByteArray wire = entry.value("method").toString().toUtf8() + ' '
        + entry.value("path").toString().toUtf8() + ' ' + entry.value("version").toString().toUtf8()
        + "\r\n";
    for (const QJsonValue& header : entry.value("headers").toArray())
    {
        const QJsonArray pair = header.toArray();
        wire += pair.at(0).toString().toUtf8() + ": " + pair.at(1).toString().toUtf8() + "\r\n";
    }
    wire += "\r\n";
    wire += entry.value("body").toString().toUtf8();
    return wire;
}

inline QByteArray wireFormatOf(const QList<QJsonObject>& entries)
{
    QByteArray wire;
    for (const QJsonObject& entry : entries)
        wire += wireFormat(entry);
    return wire;
}

inline QJsonObject withPath(QJsonObject entry, const QString& path)
{
    entry.insert("path", path);
    return entry;
}

inline QJsonObject withHeader(QJsonObject entry, const QString& name, const QString& value)
{
    QJsonArray headers;
    for (const QJsonValue& header : entry.value("headers").toArray())
    {
        const QJsonArray pair = header.toArray();
        const bool replaced = pair.at(0).toString().compare(name, Qt::CaseInsensitive) == 0;
        headers.append(replaced ? QJsonArray { name, value } : pair);
    }
    entry.insert("headers", headers);
    return entry;
}

}
