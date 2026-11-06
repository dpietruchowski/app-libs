#pragma once

#include <gtest/gtest.h>

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QTest>
#include <memory>

#include "agent/agent.h"
#include "agent/claudelauncher.h"
#include "agent/claudestdioclient.h"
#include "agent/structures.h"
#include "fixturefile.h"

namespace stdiotest
{

constexpr int kWaitMs = 10000;

inline const QString kSessionId = QStringLiteral("64174da0-2cdb-4392-b7ec-5fe0b2f28b20");

inline QString compact(const QJsonObject& object)
{
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

inline QString cliSays(const QJsonObject& event) { return "< " + compact(event); }

inline QString cliBuffers(const QJsonObject& event) { return "+ " + compact(event); }

inline QString cliRuns(const QString& command) { return "! " + command; }

inline QString nextStart() { return QStringLiteral("---"); }

inline QStringList handshake() { return { ">", ">" }; }

inline QJsonObject resultEvent(const QString& text, bool isError = false)
{
    return {
        { "type", "result" }, { "subtype", "success" }, { "is_error", isError }, { "result", text }
    };
}

inline QJsonObject initEvent(const QString& sessionId)
{
    return { { "type", "system" }, { "subtype", "init" }, { "session_id", sessionId } };
}

inline QStringList startOfRun(const QString& sessionId = kSessionId)
{
    return handshake() + QStringList { cliSays(initEvent(sessionId)) };
}

inline QStringList answering(const QString& text, const QString& sessionId = kSessionId)
{
    return startOfRun(sessionId) + QStringList { cliSays(resultEvent(text)) };
}

inline QJsonObject streamEvent(const QJsonObject& inner)
{
    return { { "type", "stream_event" }, { "event", inner } };
}

inline QJsonObject messageStartEvent() { return streamEvent({ { "type", "message_start" } }); }

inline QJsonObject textDeltaEvent(const QString& text)
{
    return streamEvent({ { "type", "content_block_delta" },
                         { "index", 0 },
                         { "delta", QJsonObject { { "type", "text_delta" }, { "text", text } } } });
}

inline QJsonObject toolCallEvent(const QString& requestId, int rpcId, const QString& tool,
                                 const QJsonObject& arguments = {})
{
    const QJsonObject message { { "jsonrpc", "2.0" },
                                { "id", rpcId },
                                { "method", "tools/call" },
                                { "params",
                                  QJsonObject { { "name", tool }, { "arguments", arguments } } } };
    return { { "type", "control_request" },
             { "request_id", requestId },
             { "request",
               QJsonObject { { "subtype", "mcp_message" },
                             { "server_name", "app" },
                             { "message", message } } } };
}

inline Message messageOf(const QString& role, const QString& content)
{
    Message message;
    message.role = role;
    message.content = content;
    return message;
}

inline Message toolMessage(const QString& callId, const QString& content)
{
    Message message = messageOf("tool", content);
    message.tool_call_id = callId;
    return message;
}

inline Messages conversationOf(const QString& userText)
{
    return { messageOf("system", "You are a test."), messageOf("user", userText) };
}

inline ModelConfig defaultConfig() { return ModelConfig { "haiku", "" }; }

inline QString userTextOf(const QJsonObject& input)
{
    return input.value("message").toObject().value("content").toString();
}

inline QString requestIdOf(const QJsonObject& input)
{
    return input.value("response").toObject().value("request_id").toString();
}

inline QJsonObject mcpReplyOf(const QJsonObject& input)
{
    return input.value("response")
        .toObject()
        .value("response")
        .toObject()
        .value("mcp_response")
        .toObject();
}

inline const QString kCourseTitles = QStringLiteral("Linux Kernel Basics\nC++ Memory Model");

inline const QString kCoursesListDocstring
    = QStringLiteral("@brief courses_list\n"
                     "@details Lists the courses of the learner.\n"
                     "@return the course titles");

inline QString requestThroughAgent(Agent& agent, const ClaudeStdioClient& client,
                                   const QString& message)
{
    bool done = false;
    QString response;
    AgentObserver observer;
    observer.finished = [&response, &done](const QString& answer)
    {
        response = answer;
        done = true;
    };
    observer.failed = [&done](const QString&) { done = true; };
    agent.setObserver(observer);

    agent.requestAsync(client, message, [](const QString&) {});
    EXPECT_TRUE(QTest::qWaitFor([&done] { return done; }, kWaitMs));
    agent.setObserver({});
    return response;
}

inline bool isAlive(qint64 pid)
{
    QFile stat(QStringLiteral("/proc/%1/stat").arg(pid));
    if (!stat.open(QIODevice::ReadOnly))
        return false;

    const QByteArray content = stat.readAll();
    const qsizetype close = content.lastIndexOf(')');
    return close >= 0 && close + 2 < content.size() && content.at(close + 2) != 'Z';
}

inline bool waitUntilDead(qint64 pid, int timeoutMs = kWaitMs)
{
    return pid > 0 && QTest::qWaitFor([pid] { return !isAlive(pid); }, timeoutMs);
}

class FakeClaude
{
public:
    explicit FakeClaude(const QStringList& script)
    {
        EXPECT_TRUE(m_directory.isValid());
        QDir().mkpath(workingDirectory());
        write(scriptPath(), (script.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8());
    }

    static FakeClaude replaying(const char* fixture, const QStringList& tail = {})
    {
        return FakeClaude(QString::fromUtf8(readFixture(fixture)).split(QLatin1Char('\n')) + tail);
    }

    ClaudeLauncher launcher() const
    {
        ClaudeLauncher launcher;
        launcher.program = QCoreApplication::applicationDirPath() + "/fake_claude";
        launcher.leadingArguments = { scriptPath(), capturePath() };
        launcher.workingDirectory = workingDirectory();
        return launcher;
    }

    QString workingDirectory() const { return QDir(m_directory.path()).filePath("work"); }

    QList<QJsonObject> records() const
    {
        QList<QJsonObject> records;
        QFile file(capturePath());
        if (!file.open(QIODevice::ReadOnly))
            return records;

        for (const QByteArray& line : file.readAll().split('\n'))
        {
            if (!line.trimmed().isEmpty())
                records.append(QJsonDocument::fromJson(line).object());
        }
        return records;
    }

    QJsonObject recordWith(const QString& key) const
    {
        for (const QJsonObject& record : records())
        {
            if (record.contains(key))
                return record;
        }
        return {};
    }

    QList<QJsonObject> recordsWith(const QString& key) const
    {
        QList<QJsonObject> matching;
        for (const QJsonObject& record : records())
        {
            if (record.contains(key))
                matching.append(record);
        }
        return matching;
    }

    bool waitForRecord(const QString& key, int timeoutMs = kWaitMs) const
    {
        return QTest::qWaitFor([this, &key] { return !recordWith(key).isEmpty(); }, timeoutMs);
    }

    int startCount() const { return recordsWith("argv").size(); }

    QStringList arguments(int run = 0) const
    {
        QStringList arguments;
        for (const QJsonValue& argument : recordsWith("argv").value(run).value("argv").toArray())
            arguments.append(argument.toString());
        return arguments;
    }

    bool hasArgument(const QString& flag, int run = 0) const
    {
        return arguments(run).contains(flag);
    }

    QString valueOf(const QString& flag, int run = 0) const
    {
        const QStringList all = arguments(run);
        const qsizetype at = all.indexOf(flag);
        return at < 0 ? QString() : all.value(at + 1);
    }

    qint64 processId(int run = 0) const
    {
        waitForRecord("argv");
        return recordsWith("argv").value(run).value("pid").toInteger();
    }

    QString startDirectory() const { return recordWith("argv").value("cwd").toString(); }

    QList<QJsonObject> inputs(int run = 0) const
    {
        QList<QJsonObject> inputs;
        int currentRun = -1;
        for (const QJsonObject& record : records())
        {
            if (record.contains("argv"))
                ++currentRun;
            const QJsonValue line = record.value("stdin");
            if (currentRun == run && line.isString())
                inputs.append(QJsonDocument::fromJson(line.toString().toUtf8()).object());
        }
        return inputs;
    }

    QString scriptPath() const { return QDir(m_directory.path()).filePath("script.txt"); }
    QString capturePath() const { return QDir(m_directory.path()).filePath("capture.jsonl"); }

private:
    static void write(const QString& path, const QByteArray& content)
    {
        QFile file(path);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(content);
    }

    QTemporaryDir m_directory;
};

struct Outcome
{
    bool done { false };
    Completion completion;
    QStringList partials;
    QStringList events;

    QString error() const { return completion.error; }

    QString text() const
    {
        return completion.choices.isEmpty() ? QString()
                                            : completion.choices.first().message.content;
    }

    QString finishReason() const
    {
        return completion.choices.isEmpty() ? QString() : completion.choices.first().finish_reason;
    }

    QVector<ToolCall> toolCalls() const
    {
        return completion.choices.isEmpty() ? QVector<ToolCall>()
                                            : completion.choices.first().message.tool_calls;
    }
};

inline std::shared_ptr<Outcome> submit(const ClaudeStdioClient& client, const Messages& messages,
                                       const ToolsMap& tools = {},
                                       const ModelConfig& config = defaultConfig())
{
    const auto outcome = std::make_shared<Outcome>();
    client.createCompletionAsync(
        config, messages, tools,
        [outcome](Completion& completion)
        {
            outcome->completion = completion;
            outcome->done = true;
            outcome->events.append("done");
        },
        [outcome](const QString& text)
        {
            outcome->partials.append(text);
            outcome->events.append("partial:" + text);
        });
    return outcome;
}

inline bool waitDone(const std::shared_ptr<Outcome>& outcome, int timeoutMs = kWaitMs)
{
    return QTest::qWaitFor([&outcome] { return outcome->done; }, timeoutMs);
}

inline Outcome ask(const ClaudeStdioClient& client, const Messages& messages,
                   const ToolsMap& tools = {}, const ModelConfig& config = defaultConfig())
{
    const std::shared_ptr<Outcome> outcome = submit(client, messages, tools, config);
    EXPECT_TRUE(waitDone(outcome)) << "the client delivered no completion";
    return *outcome;
}

}
