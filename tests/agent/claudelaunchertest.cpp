#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStringList>
#include <QTemporaryDir>

#include "agent/claudecli.h"
#include "agent/claudelauncher.h"
#include "environmentoverride.h"

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

class TemporaryHome
{
public:
    TemporaryHome()
        : m_homeDirectory(QDir(m_root.path()).filePath("home"))
        , m_override("HOME", m_homeDirectory.toUtf8())
    {
        EXPECT_TRUE(m_root.isValid());
    }

    QString root() const { return m_root.path(); }
    QString homeDirectory() const { return m_homeDirectory; }
    QString configDirectory() const { return m_homeDirectory + "/.local/share/chat-claude"; }

private:
    QTemporaryDir m_root;
    QString m_homeDirectory;
    EnvironmentOverride m_override;
};

constexpr QFileDevice::Permissions kOwnerOnly
    = QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner;
constexpr QFileDevice::Permissions kEveryoneElse = QFileDevice::ReadGroup | QFileDevice::WriteGroup
    | QFileDevice::ExeGroup | QFileDevice::ReadOther | QFileDevice::WriteOther
    | QFileDevice::ExeOther;

bool isOwnerOnly(const QString& path)
{
    const QFileDevice::Permissions permissions = QFileInfo(path).permissions();
    return (permissions & kOwnerOnly) == kOwnerOnly && !(permissions & kEveryoneElse);
}

const QString kSessionId = QStringLiteral("64174da0-2cdb-4392-b7ec-5fe0b2f28b20");

ClaudeLauncher launcherAt(const QString& configDirectory, const QString& workingDirectory)
{
    ClaudeLauncher launcher;
    if (!configDirectory.isEmpty())
        launcher.environment.insert("CLAUDE_CONFIG_DIR", configDirectory);
    launcher.workingDirectory = workingDirectory;
    return launcher;
}

}

TEST(ClaudeLauncherTest, DropsEveryAnthropicAndClaudeVariable)
{
    const QProcessEnvironment environment = claudecli::cleanEnvironment(parentEnvironment());

    for (const QString& name : { "ANTHROPIC_API_KEY", "ANTHROPIC_BASE_URL", "CLAUDE_API_KEY",
                                 "CLAUDECODE", "CLAUDE_CODE_ENTRYPOINT", "CLAUDE_CONFIG_DIR" })
        EXPECT_FALSE(environment.contains(name)) << name.toStdString();
}

TEST(ClaudeLauncherTest, KeepsTheOtherVariablesAsTheyWere)
{
    const QProcessEnvironment environment = claudecli::cleanEnvironment(parentEnvironment());

    EXPECT_EQ(environment.value("HOME"), "/home/someone");
    EXPECT_EQ(environment.value("PATH"), "/usr/bin:/bin");
    EXPECT_EQ(environment.value("LANG"), "en_US.UTF-8");
    EXPECT_EQ(environment.value("MY_CLAUDE_HOME"), "/opt/claude");
}

TEST(ClaudeLauncherTest, DisablesBackgroundTasks)
{
    EXPECT_EQ(claudecli::cleanEnvironment(parentEnvironment())
                  .value("CLAUDE_CODE_DISABLE_BACKGROUND_TASKS"),
              "1");
}

TEST(ClaudeLauncherTest, ReplacesAnInheritedBackgroundTasksSetting)
{
    QProcessEnvironment source = parentEnvironment();
    source.insert("CLAUDE_CODE_DISABLE_BACKGROUND_TASKS", "0");

    EXPECT_EQ(claudecli::cleanEnvironment(source).value("CLAUDE_CODE_DISABLE_BACKGROUND_TASKS"),
              "1");
}

TEST(ClaudeLauncherTest, GivesAnEnvironmentWithOnlyTheBackgroundSettingFromAnEmptyOne)
{
    const QProcessEnvironment environment = claudecli::cleanEnvironment(QProcessEnvironment());

    EXPECT_EQ(environment.keys(), (QStringList { "CLAUDE_CODE_DISABLE_BACKGROUND_TASKS" }));
}

TEST(ClaudeLauncherTest, BuildsTheDefaultLauncherInTheChatDirectory)
{
    const TemporaryHome data;
    const QString chatDirectory = QDir(data.root()).filePath("chat");

    const ClaudeLauncher launcher = claudecli::defaultLauncher(chatDirectory);

    EXPECT_EQ(launcher.program, claudecli::executable());
    EXPECT_TRUE(launcher.leadingArguments.isEmpty());
    EXPECT_EQ(launcher.workingDirectory, QFileInfo(chatDirectory).canonicalFilePath());
    EXPECT_EQ(launcher.idleTimeoutMs, 120000);
    EXPECT_EQ(launcher.hardTimeoutMs, 600000);
}

TEST(ClaudeLauncherTest, KeepsTheClaudeStateInTheChatClaudeDirectoryUnderTheHomeDirectory)
{
    const TemporaryHome data;

    const ClaudeLauncher launcher = claudecli::defaultLauncher(QDir(data.root()).filePath("chat"));

    const QString configDirectory = launcher.environment.value("CLAUDE_CONFIG_DIR");
    EXPECT_EQ(configDirectory, QFileInfo(data.configDirectory()).canonicalFilePath());
    EXPECT_TRUE(QFileInfo(configDirectory).isDir());
}

TEST(ClaudeLauncherTest, KeepsTheClaudeStateUnderTheHomeDirectoryWhateverTheDataLocationIs)
{
    const TemporaryHome data;
    const QString elsewhere = QDir(data.root()).filePath("elsewhere");
    const EnvironmentOverride dataHome("XDG_DATA_HOME", elsewhere.toUtf8());

    const ClaudeLauncher launcher = claudecli::defaultLauncher(QDir(data.root()).filePath("chat"));

    EXPECT_EQ(launcher.environment.value("CLAUDE_CONFIG_DIR"),
              QFileInfo(data.configDirectory()).canonicalFilePath());
    EXPECT_FALSE(QFileInfo::exists(elsewhere));
}

TEST(ClaudeLauncherTest, SharesTheClaudeStateBetweenTheChatDirectoriesOfDifferentApps)
{
    const TemporaryHome data;

    const ClaudeLauncher first = claudecli::defaultLauncher(QDir(data.root()).filePath("one"));
    const ClaudeLauncher second = claudecli::defaultLauncher(QDir(data.root()).filePath("two"));

    EXPECT_NE(first.workingDirectory, second.workingDirectory);
    EXPECT_EQ(first.environment.value("CLAUDE_CONFIG_DIR"),
              second.environment.value("CLAUDE_CONFIG_DIR"));
}

TEST(ClaudeLauncherTest, CreatesMissingParentDirectories)
{
    const TemporaryHome data;

    claudecli::defaultLauncher(QDir(data.root()).filePath("data/app/chat"));

    EXPECT_TRUE(QFileInfo(data.root() + "/data/app/chat").isDir());
    EXPECT_TRUE(QFileInfo(data.configDirectory()).isDir());
}

TEST(ClaudeLauncherTest, MakesTheChatAndConfigDirectoriesPrivateToTheOwner)
{
    const TemporaryHome data;
    const QString chatDirectory = QDir(data.root()).filePath("chat");

    claudecli::defaultLauncher(chatDirectory);

    EXPECT_TRUE(isOwnerOnly(chatDirectory));
    EXPECT_TRUE(isOwnerOnly(data.configDirectory()));
}

TEST(ClaudeLauncherTest, TightensDirectoriesThatAlreadyExistWithWiderPermissions)
{
    const TemporaryHome data;
    const QString chatDirectory = QDir(data.root()).filePath("chat");
    const QString configDirectory = data.configDirectory();
    ASSERT_TRUE(QDir().mkpath(chatDirectory));
    ASSERT_TRUE(QDir().mkpath(configDirectory));
    ASSERT_TRUE(QFile::setPermissions(chatDirectory,
                                      kOwnerOnly | QFileDevice::ReadGroup | QFileDevice::ExeGroup));
    ASSERT_TRUE(QFile::setPermissions(configDirectory,
                                      kOwnerOnly | QFileDevice::ReadGroup | QFileDevice::WriteGroup
                                          | QFileDevice::ExeGroup | QFileDevice::ReadOther
                                          | QFileDevice::ExeOther));

    ASSERT_FALSE(isOwnerOnly(chatDirectory));
    ASSERT_FALSE(isOwnerOnly(configDirectory));

    claudecli::defaultLauncher(chatDirectory);

    EXPECT_TRUE(isOwnerOnly(chatDirectory));
    EXPECT_TRUE(isOwnerOnly(configDirectory));
}

TEST(ClaudeLauncherTest, KeepsTheCredentialsInAnExistingConfigDirectoryUntouched)
{
    const TemporaryHome data;
    const QString configDirectory = data.configDirectory();
    ASSERT_TRUE(QDir().mkpath(configDirectory));
    const QString credentials = configDirectory + "/.credentials.json";
    QFile file(credentials);
    ASSERT_TRUE(file.open(QIODevice::WriteOnly));
    file.write("kept");
    file.close();

    claudecli::defaultLauncher(QDir(data.root()).filePath("chat"));

    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    EXPECT_EQ(file.readAll(), "kept");
}

TEST(ClaudeLauncherTest, ReplacesAnInheritedConfigDirectoryWithTheSharedOne)
{
    const TemporaryHome data;
    const EnvironmentOverride inherited("CLAUDE_CONFIG_DIR", "/elsewhere");

    const ClaudeLauncher launcher = claudecli::defaultLauncher(QDir(data.root()).filePath("chat"));

    EXPECT_EQ(launcher.environment.value("CLAUDE_CONFIG_DIR"),
              QFileInfo(data.configDirectory()).canonicalFilePath());
}

TEST(ClaudeLauncherTest, BuildsTheDefaultEnvironmentFromTheCleanProcessEnvironment)
{
    const TemporaryHome home;
    const EnvironmentOverride key("ANTHROPIC_API_KEY", "secret");

    const ClaudeLauncher launcher = claudecli::defaultLauncher(QDir(home.root()).filePath("chat"));

    EXPECT_EQ(launcher.environment.value("HOME"), home.homeDirectory());
    EXPECT_EQ(launcher.environment.value("CLAUDE_CODE_DISABLE_BACKGROUND_TASKS"), "1");
    for (const QString& name : launcher.environment.keys())
    {
        EXPECT_FALSE(name.startsWith("ANTHROPIC_")) << name.toStdString();
        if (name.startsWith("CLAUDE"))
        {
            EXPECT_TRUE(name == "CLAUDE_CODE_DISABLE_BACKGROUND_TASKS"
                        || name == "CLAUDE_CONFIG_DIR")
                << name.toStdString();
        }
    }
}

TEST(ClaudeLauncherTest, PutsTheSessionFileInTheProjectDirectoryOfTheWorkingDirectory)
{
    const ClaudeLauncher launcher = launcherAt("/cfg", "/data/chat");

    EXPECT_EQ(claudecli::sessionFile(launcher, kSessionId).toStdString(),
              ("/cfg/projects/-data-chat/" + kSessionId + ".jsonl").toStdString());
}

TEST(ClaudeLauncherTest, ReplacesEveryCharacterOutsideAsciiLettersAndDigitsWithADash)
{
    const ClaudeLauncher launcher = launcherAt("/cfg", "/Data/app.chat_1/My Chat");

    EXPECT_EQ(claudecli::sessionFile(launcher, kSessionId).toStdString(),
              ("/cfg/projects/-Data-app-chat-1-My-Chat/" + kSessionId + ".jsonl").toStdString());
}

TEST(ClaudeLauncherTest, ReplacesEveryUtf16UnitOfANonAsciiCharacterWithADash)
{
    const ClaudeLauncher launcher = launcherAt(
        "/cfg", QString::fromUtf8("/dane/za\xC5\xBC\xC3\xB3\xC5\x82\xC4\x87/\xF0\x9F\x98\x80"));

    const QString projectName = "-dane-za" + QString(4 + 1 + 2, QLatin1Char('-'));
    EXPECT_EQ(claudecli::sessionFile(launcher, kSessionId).toStdString(),
              ("/cfg/projects/" + projectName + "/" + kSessionId + ".jsonl").toStdString());
}

TEST(ClaudeLauncherTest, GivesNoSessionFileWithoutAConfigDirectory)
{
    EXPECT_TRUE(claudecli::sessionFile(launcherAt({}, "/data/chat"), kSessionId).isEmpty());
}

TEST(ClaudeLauncherTest, GivesNoSessionFileWithoutAWorkingDirectory)
{
    EXPECT_TRUE(claudecli::sessionFile(launcherAt("/cfg", {}), kSessionId).isEmpty());
}

TEST(ClaudeLauncherTest, GivesNoSessionFileForAnIdThatIsNotAUuid)
{
    const ClaudeLauncher launcher = launcherAt("/cfg", "/data/chat");

    for (const QString& id : { QString(), QString("../../.claude.json"), QString("abc"),
                               kSessionId + "/../x", kSessionId + "\n" })
        EXPECT_TRUE(claudecli::sessionFile(launcher, id).isEmpty()) << id.toStdString();
}

TEST(ClaudeLauncherTest, NeverPointsOutsideTheConfigDirectory)
{
    const ClaudeLauncher launcher = launcherAt("/cfg", "/data/chat");

    EXPECT_TRUE(claudecli::sessionFile(launcher, kSessionId).startsWith("/cfg/projects/"));
}

TEST(ClaudeLauncherTest, LogsInWithTheConfigDirectoryOfTheLauncher)
{
    const ClaudeLauncher launcher = launcherAt("/cfg", "/data/chat");

    EXPECT_EQ(claudecli::loginCommand(launcher).toStdString(),
              "env CLAUDE_CONFIG_DIR=/cfg claude auth login");
}
