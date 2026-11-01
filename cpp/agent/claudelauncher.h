#pragma once

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

struct ClaudeLauncher
{
    QString program;
    QStringList leadingArguments;
    QProcessEnvironment environment;
    QString workingDirectory;
    int idleTimeoutMs { 120000 };
    int hardTimeoutMs { 600000 };
};

namespace claudecli
{

QProcessEnvironment cleanEnvironment(const QProcessEnvironment& source);

ClaudeLauncher defaultLauncher(const QString& chatDirectory);

QString sessionFile(const ClaudeLauncher& launcher, const QString& id);

}
