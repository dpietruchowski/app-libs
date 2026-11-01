#include "claudelauncher.h"

#include "claudecli.h"

namespace
{

const QString kApiPrefix = QStringLiteral("ANTHROPIC_");
const QString kClaudePrefix = QStringLiteral("CLAUDE");
const QString kDisableBackgroundTasks = QStringLiteral("CLAUDE_CODE_DISABLE_BACKGROUND_TASKS");
const QString kEnabled = QStringLiteral("1");

bool isInherited(const QString& name)
{
    return !name.startsWith(kApiPrefix) && !name.startsWith(kClaudePrefix);
}

}

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

ClaudeLauncher defaultLauncher()
{
    ClaudeLauncher launcher;
    launcher.program = claudecli::executable();
    launcher.environment = cleanEnvironment(QProcessEnvironment::systemEnvironment());
    return launcher;
}
