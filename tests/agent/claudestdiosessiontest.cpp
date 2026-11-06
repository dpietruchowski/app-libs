#include <gtest/gtest.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#include "agent/agent.h"
#include "agent/claudecli.h"
#include "agent/claudestdioclient.h"
#include "claudestdiosupport.h"

using namespace stdiotest;

namespace
{

const QString kOtherSessionId = QStringLiteral("9d2c5ef1-0a2f-4d6f-8d58-0d3c9ea8c5a1");
const QString kConfigDirectoryVariable = QStringLiteral("CLAUDE_CONFIG_DIR");

Messages history()
{
    return { messageOf("system", "You are a test."), messageOf("user", "first question"),
             messageOf("assistant", "first answer"), messageOf("user", "second question") };
}

ClaudeSession keeping(const QString& resume = {}) { return ClaudeSession { resume, true }; }

QStringList missingSession()
{
    return QString::fromUtf8(readFixture("stdio-missing-session.jsonl")).split(QLatin1Char('\n'));
}

QStringList toolRun()
{
    return QString::fromUtf8(readFixture("stdio-tool-run.jsonl")).split(QLatin1Char('\n'));
}

QStringList afterMissingSession(const QStringList& retry)
{
    return missingSession() + QStringList { nextStart() } + retry;
}

QString sentText(const FakeClaude& claude, int run)
{
    const QList<QJsonObject> inputs = claude.inputs(run);
    return inputs.size() < 2 ? QString() : userTextOf(inputs[1]);
}

struct ForgetFixture
{
    ForgetFixture()
    {
        EXPECT_TRUE(configDirectory.isValid());
        EXPECT_TRUE(workingDirectory.isValid());
        launcher.environment.insert(kConfigDirectoryVariable, configDirectory.path());
        launcher.workingDirectory = workingDirectory.path();
    }

    QString create(const QString& path) const
    {
        EXPECT_TRUE(QDir().mkpath(QFileInfo(path).absolutePath()));
        QFile file(path);
        EXPECT_TRUE(file.open(QIODevice::WriteOnly));
        file.write("{}\n");
        return path;
    }

    QString createSession(const QString& id) const
    {
        return create(claudecli::sessionFile(launcher, id));
    }

    QTemporaryDir configDirectory;
    QTemporaryDir workingDirectory;
    ClaudeLauncher launcher;
};

}

TEST(ClaudeStdioSessionTest, PersistsNothingAndReportsNoSessionWithoutKeep)
{
    const FakeClaude claude(answering("pong"));
    const ClaudeStdioClient client(claude.launcher(), ClaudeSession { kSessionId, false });

    ask(client, history());

    EXPECT_TRUE(claude.hasArgument("--no-session-persistence"));
    EXPECT_FALSE(claude.hasArgument("--resume"));
    EXPECT_FALSE(claude.hasArgument("--session-id"));
    EXPECT_EQ(sentText(claude, 0), claudecli::conversation(history()));
    EXPECT_TRUE(client.session().isEmpty());
}

TEST(ClaudeStdioSessionTest, SeedsANewSessionWhenKeepingWithoutAnId)
{
    const FakeClaude claude(answering("pong"));
    const ClaudeStdioClient client(claude.launcher(), keeping());

    ask(client, history());

    EXPECT_TRUE(claudecli::isSessionId(claude.valueOf("--session-id")));
    EXPECT_FALSE(claude.hasArgument("--resume"));
    EXPECT_FALSE(claude.hasArgument("--no-session-persistence"));
    EXPECT_EQ(sentText(claude, 0), claudecli::conversation(history()));
}

TEST(ClaudeStdioSessionTest, SeedsADifferentSessionForEveryClient)
{
    const FakeClaude first(answering("pong"));
    const FakeClaude second(answering("pong"));
    const ClaudeStdioClient firstClient(first.launcher(), keeping());
    const ClaudeStdioClient secondClient(second.launcher(), keeping());

    ask(firstClient, history());
    ask(secondClient, history());

    ASSERT_FALSE(first.valueOf("--session-id").isEmpty());
    EXPECT_NE(first.valueOf("--session-id"), second.valueOf("--session-id"));
}

TEST(ClaudeStdioSessionTest, ResumesTheGivenSessionWithTheLastUserMessageOnly)
{
    const FakeClaude claude(answering("pong"));
    const ClaudeStdioClient client(claude.launcher(), keeping(kSessionId));

    ask(client, history());

    EXPECT_EQ(claude.valueOf("--resume"), kSessionId);
    EXPECT_FALSE(claude.hasArgument("--session-id"));
    EXPECT_FALSE(claude.hasArgument("--no-session-persistence"));
    EXPECT_EQ(sentText(claude, 0), "second question");
}

TEST(ClaudeStdioSessionTest, ResumesWithAnEmptyMessageWhenThereIsNoUserMessage)
{
    const FakeClaude claude(answering("pong"));
    const ClaudeStdioClient client(claude.launcher(), keeping(kSessionId));

    ask(client, { messageOf("system", "You are a test.") });

    EXPECT_EQ(claude.valueOf("--resume"), kSessionId);
    EXPECT_EQ(claude.inputs(0).size(), 2);
    EXPECT_TRUE(sentText(claude, 0).isEmpty());
}

TEST(ClaudeStdioSessionTest, SeedsANewSessionInsteadOfResumingAnIdThatIsNotAUuid)
{
    const FakeClaude claude(answering("pong"));
    const ClaudeStdioClient client(claude.launcher(), keeping("--dangerously-skip-permissions"));

    ask(client, history());

    EXPECT_FALSE(claude.hasArgument("--resume"));
    EXPECT_FALSE(claude.hasArgument("--dangerously-skip-permissions"));
    EXPECT_TRUE(claudecli::isSessionId(claude.valueOf("--session-id")));
    EXPECT_EQ(sentText(claude, 0), claudecli::conversation(history()));
}

TEST(ClaudeStdioSessionTest, ReportsTheSessionItWasGivenBeforeTheRun)
{
    const FakeClaude claude(answering("pong"));

    EXPECT_EQ(ClaudeStdioClient(claude.launcher(), keeping(kSessionId)).session(), kSessionId);
    EXPECT_TRUE(ClaudeStdioClient(claude.launcher(), keeping()).session().isEmpty());
    EXPECT_TRUE(ClaudeStdioClient(claude.launcher(), keeping("not-a-session")).session().isEmpty());
    EXPECT_TRUE(ClaudeStdioClient(claude.launcher(), ClaudeSession { kSessionId, false })
                    .session()
                    .isEmpty());
    EXPECT_TRUE(ClaudeStdioClient(claude.launcher()).session().isEmpty());
}

TEST(ClaudeStdioSessionTest, ReportsTheSessionTheCliStartedThroughTheClientInterface)
{
    const FakeClaude claude(answering("pong", kOtherSessionId));
    const ClaudeStdioClient client(claude.launcher(), keeping());
    const Client& base = client;

    ask(client, history());

    EXPECT_EQ(base.session(), kOtherSessionId);
    EXPECT_NE(base.session(), claude.valueOf("--session-id"));
}

TEST(ClaudeStdioSessionTest, KeepsTheResumedSessionWhenTheCliConfirmsIt)
{
    const FakeClaude claude(answering("pong"));
    const ClaudeStdioClient client(claude.launcher(), keeping(kSessionId));

    ask(client, history());

    EXPECT_EQ(client.session(), kSessionId);
}

TEST(ClaudeStdioSessionTest, ReportsNoSessionWhenTheSessionIsNotKept)
{
    const FakeClaude claude(answering("pong", kOtherSessionId));
    const ClaudeStdioClient client(claude.launcher());

    ask(client, history());

    EXPECT_TRUE(client.session().isEmpty());
}

TEST(ClaudeStdioSessionTest, ReportsNoSessionWhenAFreshRunNeverStarted)
{
    const FakeClaude claude(answering("pong"));
    ClaudeLauncher launcher = claude.launcher();
    launcher.program.clear();
    const ClaudeStdioClient client(launcher, keeping());

    const Outcome outcome = ask(client, history());

    EXPECT_EQ(outcome.error(), claudecli::kNetworkError);
    EXPECT_TRUE(client.session().isEmpty());
}

TEST(ClaudeStdioSessionTest, SeedsANewSessionWhenTheResumedOneIsMissing)
{
    const FakeClaude claude(afterMissingSession(answering("fresh", kOtherSessionId)));
    const ClaudeStdioClient client(claude.launcher(), keeping(kSessionId));

    const Outcome outcome = ask(client, history());

    EXPECT_TRUE(outcome.error().isEmpty());
    EXPECT_EQ(outcome.text(), "fresh");
    ASSERT_EQ(claude.startCount(), 2);
    EXPECT_EQ(claude.valueOf("--resume", 0), kSessionId);
    EXPECT_FALSE(claude.hasArgument("--session-id", 0));
    EXPECT_TRUE(claudecli::isSessionId(claude.valueOf("--session-id", 1)));
    EXPECT_NE(claude.valueOf("--session-id", 1), kSessionId);
    EXPECT_FALSE(claude.hasArgument("--resume", 1));
    EXPECT_FALSE(claude.hasArgument("--no-session-persistence", 1));
}

TEST(ClaudeStdioSessionTest, SendsTheWholeConversationToTheReseededRun)
{
    const FakeClaude claude(afterMissingSession(answering("fresh", kOtherSessionId)));
    const ClaudeStdioClient client(claude.launcher(), keeping(kSessionId));

    ask(client, history());

    EXPECT_EQ(sentText(claude, 0), "second question");
    EXPECT_EQ(sentText(claude, 1), claudecli::conversation(history()));
    EXPECT_EQ(claude.inputs(1).first().value("request").toObject().value("subtype").toString(),
              "initialize");
}

TEST(ClaudeStdioSessionTest, ReportsTheSessionOfTheReseededRun)
{
    const FakeClaude claude(afterMissingSession(answering("fresh", kOtherSessionId)));
    const ClaudeStdioClient client(claude.launcher(), keeping(kSessionId));

    ask(client, history());

    EXPECT_EQ(client.session(), kOtherSessionId);
}

TEST(ClaudeStdioSessionTest, KillsTheProcessThatLackedItsSession)
{
    const FakeClaude claude(missingSession() + QStringList { cliRuns("hang"), nextStart() }
                            + answering("fresh", kOtherSessionId));
    const ClaudeStdioClient client(claude.launcher(), keeping(kSessionId));

    const Outcome outcome = ask(client, history());

    EXPECT_EQ(outcome.text(), "fresh");
    ASSERT_EQ(claude.startCount(), 2);
    EXPECT_TRUE(waitUntilDead(claude.processId(0)));
}

TEST(ClaudeStdioSessionTest, StreamsTheTextOfTheReseededRun)
{
    const FakeClaude claude(afterMissingSession(
        startOfRun(kOtherSessionId)
        + QStringList { cliSays(messageStartEvent()), cliSays(textDeltaEvent("Hel")),
                        cliSays(textDeltaEvent("lo")), cliSays(resultEvent("Hello")) }));
    const ClaudeStdioClient client(claude.launcher(), keeping(kSessionId));

    const std::shared_ptr<Outcome> outcome = submit(client, history());
    ASSERT_TRUE(waitDone(outcome));

    EXPECT_EQ(outcome->partials, (QStringList { "Hel", "Hello" }));
    EXPECT_EQ(outcome->text(), "Hello");
}

TEST(ClaudeStdioSessionTest, KeepsTheHardLimitOnTheReseededRunWhenTheOldProcessExitsAtOnce)
{
    const FakeClaude claude(missingSession() + QStringList { cliRuns("exit 0"), nextStart() }
                            + startOfRun(kOtherSessionId) + QStringList { cliRuns("hang") });
    ClaudeLauncher launcher = claude.launcher();
    launcher.hardTimeoutMs = 1000;
    const ClaudeStdioClient client(launcher, keeping(kSessionId));

    const Outcome outcome = ask(client, history());

    EXPECT_EQ(outcome.error(), claudecli::kTimeout);
    EXPECT_EQ(claude.startCount(), 2);
    EXPECT_TRUE(waitUntilDead(claude.processId(1)));
}

TEST(ClaudeStdioSessionTest, ServesTheToolsAfterTheSessionWasReseeded)
{
    const FakeClaude claude(afterMissingSession(toolRun()));
    const ClaudeStdioClient client(claude.launcher(), keeping(kSessionId));
    Agent agent("haiku", "You are a test.");
    int toolCalls = 0;
    agent.addTool(kCoursesListDocstring,
                  [&toolCalls](QVariantMap, ToolDone done)
                  {
                      ++toolCalls;
                      done(kCourseTitles);
                  });

    const QString response = requestThroughAgent(agent, client, "Which courses do I have?");

    EXPECT_EQ(response, "Your courses:\n\n- Linux Kernel Basics\n- C++ Memory Model");
    EXPECT_EQ(toolCalls, 1);
    EXPECT_EQ(claude.startCount(), 2);
    EXPECT_EQ(claude.inputs(1).size(), 6);
}

TEST(ClaudeStdioSessionTest, GivesUpWhenTheReseededSessionIsMissingToo)
{
    const FakeClaude claude(afterMissingSession(missingSession()));
    const ClaudeStdioClient client(claude.launcher(), keeping(kSessionId));

    const Outcome outcome = ask(client, history());

    EXPECT_EQ(outcome.error(), claudecli::kServerError);
    EXPECT_EQ(claude.startCount(), 2);
}

TEST(ClaudeStdioSessionTest, FailsWithoutRetryWhenASeededSessionIsMissing)
{
    const FakeClaude claude(afterMissingSession(answering("unused")));
    const ClaudeStdioClient client(claude.launcher(), keeping());

    const Outcome outcome = ask(client, history());

    EXPECT_EQ(outcome.error(), claudecli::kServerError);
    EXPECT_EQ(claude.startCount(), 1);
}

TEST(ClaudeStdioSessionTest, FailsWithoutRetryWhenAnUnkeptSessionIsMissing)
{
    const FakeClaude claude(afterMissingSession(answering("unused")));
    const ClaudeStdioClient client(claude.launcher(), ClaudeSession { kSessionId, false });

    const Outcome outcome = ask(client, history());

    EXPECT_EQ(outcome.error(), claudecli::kServerError);
    EXPECT_EQ(claude.startCount(), 1);
}

TEST(ClaudeStdioSessionTest, ForgetRemovesTheSessionFileOfTheLauncher)
{
    const ForgetFixture fixture;
    const QString path = fixture.createSession(kSessionId);
    ASSERT_TRUE(QFile::exists(path));

    ClaudeStdioClient::forget(fixture.launcher, kSessionId);

    EXPECT_FALSE(QFile::exists(path));
}

TEST(ClaudeStdioSessionTest, ForgetLeavesTheOtherSessionsAlone)
{
    const ForgetFixture fixture;
    const QString forgotten = fixture.createSession(kSessionId);
    const QString other = fixture.createSession(kOtherSessionId);

    ClaudeStdioClient::forget(fixture.launcher, kSessionId);

    EXPECT_FALSE(QFile::exists(forgotten));
    EXPECT_TRUE(QFile::exists(other));
}

TEST(ClaudeStdioSessionTest, ForgetIgnoresAnIdThatIsNotASessionId)
{
    const ForgetFixture fixture;
    const QString outside
        = fixture.create(QDir(fixture.configDirectory.path()).filePath("outside.jsonl"));
    const QString sibling = fixture.create(
        QFileInfo(claudecli::sessionFile(fixture.launcher, kSessionId)).absolutePath()
        + "/victim.jsonl");

    ClaudeStdioClient::forget(fixture.launcher, "../../outside");
    ClaudeStdioClient::forget(fixture.launcher, "victim");
    ClaudeStdioClient::forget(fixture.launcher, QString());

    EXPECT_TRUE(QFile::exists(outside));
    EXPECT_TRUE(QFile::exists(sibling));
}

TEST(ClaudeStdioSessionTest, ForgetDoesNothingWithoutAConfigDirectory)
{
    const ForgetFixture fixture;
    ClaudeLauncher launcher = fixture.launcher;
    launcher.environment.remove(kConfigDirectoryVariable);
    const QString decoy
        = fixture.create(QDir(launcher.workingDirectory).filePath(kSessionId + ".jsonl"));

    ClaudeStdioClient::forget(launcher, kSessionId);

    EXPECT_TRUE(QFile::exists(decoy));
}
