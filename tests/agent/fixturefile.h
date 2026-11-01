#pragma once

#include <gtest/gtest.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QList>
#include <QString>

inline QByteArray readFixture(const char* name)
{
    const QDir directory(QFileInfo(QString::fromUtf8(__FILE__)).absolutePath());
    QFile file(directory.filePath(QStringLiteral("fixtures/") + QString::fromUtf8(name)));
    if (!file.open(QIODevice::ReadOnly))
    {
        ADD_FAILURE() << "cannot open fixture " << name;
        return {};
    }
    return file.readAll();
}

constexpr char kCliSays = '<';
constexpr char kHostSays = '>';

inline QList<QByteArray> readFixtureSide(const char* name, char side)
{
    QList<QByteArray> lines;
    for (const QByteArray& line : readFixture(name).split('\n'))
    {
        if (line.size() > 2 && line.at(0) == side && line.at(1) == ' ')
            lines.append(line.sliced(2));
    }
    return lines;
}

inline QByteArray readFixtureStream(const char* name, char side)
{
    QByteArray stream;
    for (const QByteArray& line : readFixtureSide(name, side))
        stream += line + '\n';
    return stream;
}
