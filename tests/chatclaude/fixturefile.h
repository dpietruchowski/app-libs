#pragma once

#include <gtest/gtest.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>

namespace chat::test
{

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

}
