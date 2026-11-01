#include "claudelauncher.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include "claudecli.h"
#include "logging.h"

namespace
{

const QString kApiPrefix = QStringLiteral("ANTHROPIC_");
const QString kClaudePrefix = QStringLiteral("CLAUDE");
const QString kDisableBackgroundTasks = QStringLiteral("CLAUDE_CODE_DISABLE_BACKGROUND_TASKS");
const QString kConfigDirectory = QStringLiteral("CLAUDE_CONFIG_DIR");
const QString kEnabled = QStringLiteral("1");
const QString kSharedConfigDirectoryName = QStringLiteral("chat-claude");

bool isInherited(const QString& name)
{
    return !name.startsWith(kApiPrefix) && !name.startsWith(kClaudePrefix);
}

QString privateDirectory(const QString& path)
{
    QDir().mkpath(path);
    const bool restricted = QFile::setPermissions(
        path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    if (!restricted)
        qCWarning(ClientLogic) << "cannot restrict the permissions of" << path;
    const QString resolved = QFileInfo(path).canonicalFilePath();
    return resolved.isEmpty() ? QDir::cleanPath(QFileInfo(path).absoluteFilePath()) : resolved;
}

QString sharedConfigDirectory()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
        .filePath(kSharedConfigDirectoryName);
}

bool isAsciiAlphanumeric(QChar character)
{
    const ushort code = character.unicode();
    return (code >= '0' && code <= '9') || (code >= 'A' && code <= 'Z')
        || (code >= 'a' && code <= 'z');
}

QString projectName(QString directory)
{
    for (QChar& character : directory)
    {
        if (!isAsciiAlphanumeric(character))
            character = QLatin1Char('-');
    }
    return directory;
}

}

namespace claudecli
{

QProcessEnvironment cleanEnvironment(const QProcessEnvironment& source)
{
    QProcessEnvironment environment;
    for (const QString& name : source.keys())
    {
        if (isInherited(name))
            environment.insert(name, source.value(name));
    }
    environment.insert(kDisableBackgroundTasks, kEnabled);
    return environment;
}

ClaudeLauncher defaultLauncher(const QString& chatDirectory)
{
    ClaudeLauncher launcher;
    launcher.program = executable();
    launcher.workingDirectory = privateDirectory(chatDirectory);
    launcher.environment = cleanEnvironment(QProcessEnvironment::systemEnvironment());
    launcher.environment.insert(kConfigDirectory, privateDirectory(sharedConfigDirectory()));
    return launcher;
}

QString sessionFile(const ClaudeLauncher& launcher, const QString& id)
{
    const QString configDirectory = launcher.environment.value(kConfigDirectory);
    if (configDirectory.isEmpty() || launcher.workingDirectory.isEmpty() || !isSessionId(id))
        return QString();

    return QDir(configDirectory)
        .filePath(
            QStringLiteral("projects/%1/%2.jsonl").arg(projectName(launcher.workingDirectory), id));
}

QString loginCommand(const ClaudeLauncher& launcher)
{
    return QStringLiteral("env %1=%2 claude auth login")
        .arg(kConfigDirectory, launcher.environment.value(kConfigDirectory));
}

}
