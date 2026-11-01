#include <gtest/gtest.h>

#include <QProcessEnvironment>
#include <QStringList>

#include "agent/claudecli.h"
#include "agent/claudelauncher.h"

namespace
{

QProcessEnvironment parentEnvironment()
{
    QProcessEnvironment environment;
    environment.insert("HOME", "/home/someone");
    environment.insert("PATH", "/usr/bin:/bin");
    environment.insert("LANG", "en_US.UTF-8");
    environment.insert("MY_CLAUDE_HOME", "/opt/claude");
    environment.insert("ANTHROPIC_API_KEY", "secret");
    environment.insert("ANTHROPIC_BASE_URL", "https://example.invalid");
    environment.insert("CLAUDE_API_KEY", "secret");
    environment.insert("CLAUDECODE", "1");
    environment.insert("CLAUDE_CODE_ENTRYPOINT", "cli");
    environment.insert("CLAUDE_CONFIG_DIR", "/elsewhere");
    return environment;
}

}

TEST(ClaudeLauncherTest, DropsEveryAnthropicAndClaudeVariable)
{
    const QProcessEnvironment environment = cleanEnvironment(parentEnvironment());

    for (const QString& name : { "ANTHROPIC_API_KEY", "ANTHROPIC_BASE_URL", "CLAUDE_API_KEY",
                                 "CLAUDECODE", "CLAUDE_CODE_ENTRYPOINT", "CLAUDE_CONFIG_DIR" })
        EXPECT_FALSE(environment.contains(name)) << name.toStdString();
}

TEST(ClaudeLauncherTest, KeepsTheOtherVariablesAsTheyWere)
{
    const QProcessEnvironment environment = cleanEnvironment(parentEnvironment());

    EXPECT_EQ(environment.value("HOME"), "/home/someone");
    EXPECT_EQ(environment.value("PATH"), "/usr/bin:/bin");
    EXPECT_EQ(environment.value("LANG"), "en_US.UTF-8");
    EXPECT_EQ(environment.value("MY_CLAUDE_HOME"), "/opt/claude");
}

TEST(ClaudeLauncherTest, DisablesBackgroundTasks)
{
    EXPECT_EQ(cleanEnvironment(parentEnvironment()).value("CLAUDE_CODE_DISABLE_BACKGROUND_TASKS"),
              "1");
}

TEST(ClaudeLauncherTest, ReplacesAnInheritedBackgroundTasksSetting)
{
    QProcessEnvironment source = parentEnvironment();
    source.insert("CLAUDE_CODE_DISABLE_BACKGROUND_TASKS", "0");

    EXPECT_EQ(cleanEnvironment(source).value("CLAUDE_CODE_DISABLE_BACKGROUND_TASKS"), "1");
}

TEST(ClaudeLauncherTest, GivesAnEnvironmentWithOnlyTheBackgroundSettingFromAnEmptyOne)
{
    const QProcessEnvironment environment = cleanEnvironment(QProcessEnvironment());

    EXPECT_EQ(environment.keys(), (QStringList { "CLAUDE_CODE_DISABLE_BACKGROUND_TASKS" }));
}

TEST(ClaudeLauncherTest, BuildsTheDefaultLauncherFromTheCleanProcessEnvironment)
{
    const ClaudeLauncher launcher = defaultLauncher();

    EXPECT_EQ(launcher.program, claudecli::executable());
    EXPECT_TRUE(launcher.leadingArguments.isEmpty());
    EXPECT_TRUE(launcher.workingDirectory.isEmpty());
    EXPECT_EQ(launcher.idleTimeoutMs, 120000);
    EXPECT_EQ(launcher.hardTimeoutMs, 600000);
    EXPECT_EQ(launcher.environment.value("CLAUDE_CODE_DISABLE_BACKGROUND_TASKS"), "1");
    for (const QString& name : launcher.environment.keys())
    {
        EXPECT_FALSE(name.startsWith("ANTHROPIC_")) << name.toStdString();
        if (name.startsWith("CLAUDE"))
            EXPECT_EQ(name, "CLAUDE_CODE_DISABLE_BACKGROUND_TASKS");
    }
}
