#include "claudestdioclient.h"

#include <optional>
#include <utility>
#include <vector>

#include <QEventLoop>
#include <QFile>
#include <QHash>
#include <QJsonValue>
#include <QProcess>
#include <QTimer>
#include <QUuid>

#ifdef Q_OS_UNIX
#include <csignal>
#include <sys/types.h>
#include <unistd.h>
#endif
#ifdef Q_OS_LINUX
#include <sys/prctl.h>
#endif

#include "claudecli.h"
#include "logging.h"
#include "mcpdispatcher.h"
#include "streamjsonparser.h"
#include "streamjsonwriter.h"

namespace
{

const QString kFinishStop = QStringLiteral("stop");
const QString kFinishToolCalls = QStringLiteral("tool_calls");
const QString kAssistantRole = QStringLiteral("assistant");
const QString kUserRole = QStringLiteral("user");
const QString kToolRole = QStringLiteral("tool");
const QString kMcpConfig = QStringLiteral(R"({"mcpServers":{"app":{"type":"sdk","name":"app"}}})");

enum class SessionMode
{
    Seed,
    Resume,
    Ephemeral
};

struct SessionPlan
{
    SessionMode mode { SessionMode::Ephemeral };
    QString id;
};

struct Request
{
    ModelConfig config;
    Messages messages;
    ToolsMap tools;
};

struct ToolRequest
{
    ToolCall call;
    QJsonValue rpcId;
};

QString lastUserMessage(const Messages& messages)
{
    for (auto it = messages.crbegin(); it != messages.crend(); ++it)
    {
        if (it->role == kUserRole)
            return it->content;
    }
    return QString();
}

QString transcriptFor(const Request& request, SessionMode mode)
{
    return mode == SessionMode::Resume ? lastUserMessage(request.messages)
                                       : claudecli::conversation(request.messages);
}

QStringList sessionArguments(const SessionPlan& plan)
{
    switch (plan.mode)
    {
        case SessionMode::Seed:
            return { QStringLiteral("--session-id"), plan.id };
        case SessionMode::Resume:
            return { QStringLiteral("--resume"), plan.id };
        case SessionMode::Ephemeral:
            break;
    }
    return { QStringLiteral("--no-session-persistence") };
}

QStringList commandLine(const ClaudeLauncher& launcher, const Request& request,
                        const SessionPlan& plan)
{
    QStringList args = launcher.leadingArguments;
    args << QStringLiteral("-p") << QStringLiteral("--input-format")
         << QStringLiteral("stream-json") << QStringLiteral("--output-format")
         << QStringLiteral("stream-json") << QStringLiteral("--verbose")
         << QStringLiteral("--include-partial-messages")
         << QStringLiteral("--system-prompt-snapshot") << QStringLiteral("off");
    args << sessionArguments(plan);
    args << QStringLiteral("--model") << claudecli::modelOf(request.config);

    const QString effort = claudecli::effortOf(request.config);
    if (!effort.isEmpty())
        args << QStringLiteral("--effort") << effort;

    args << QStringLiteral("--system-prompt")
         << claudecli::systemMessages(request.messages).trimmed() << QStringLiteral("--restricted")
         << QStringLiteral("--tools") << QString();

    if (!request.tools.isEmpty())
    {
        args << QStringLiteral("--mcp-config") << kMcpConfig
             << QStringLiteral("--strict-mcp-config") << QStringLiteral("--allowedTools")
             << QStringLiteral("mcp__app");
    }

    args << QStringLiteral("--permission-mode") << QStringLiteral("dontAsk")
         << QStringLiteral("--permission-prompts") << QStringLiteral("none");
    return args;
}

Completion completionOf(const QString& finishReason, Message message)
{
    Choice choice {};
    choice.index = 0;
    choice.finish_reason = finishReason;
    choice.message = std::move(message);

    Completion completion {};
    completion.choices.append(std::move(choice));
    return completion;
}

Completion answerCompletion(const QString& text)
{
    Message message;
    message.role = kAssistantRole;
    message.content = text;
    return completionOf(kFinishStop, std::move(message));
}

Completion toolCallsCompletion(const std::vector<ToolRequest>& requests)
{
    Message message;
    message.role = kAssistantRole;
    for (const ToolRequest& request : requests)
        message.tool_calls.append(request.call);
    return completionOf(kFinishToolCalls, std::move(message));
}

void leaveTheApplication(QProcess& process)
{
#ifdef Q_OS_UNIX
    const pid_t application = ::getpid();
    process.setChildProcessModifier(
        [application]
        {
            ::setsid();
#ifdef Q_OS_LINUX
            ::prctl(PR_SET_PDEATHSIG, SIGKILL);
            if (::getppid() != application)
                ::_exit(1);
#endif
        });
#else
    Q_UNUSED(process)
#endif
}

void killProcessGroup(QProcess* process)
{
    if (!process)
        return;

#ifdef Q_OS_UNIX
    const qint64 pid = process->processId();
    if (pid > 0)
        ::kill(-static_cast<pid_t>(pid), SIGKILL);
#endif
    process->kill();
}

void stopTimer(QTimer* timer)
{
    if (timer)
        timer->stop();
}

}

class ClaudeStdioClient::Run : public std::enable_shared_from_this<Run>
{
public:
    Run(ClaudeLauncher launcher, ClaudeSession session);
    ~Run();

    void ask(const ModelConfig& config, const Messages& messages, const ToolsMap& toolsMap,
             const CompletionCreatedCallback& callback, const PartialTextCallback& partialText);
    QString sessionId() const;
    void shutdown();

private:
    enum class Stage
    {
        Idle,
        Running,
        AwaitingTools,
        Finished
    };

    enum class Closing
    {
        Graceful,
        Kill
    };

    void begin(const ModelConfig& config, const Messages& messages, const ToolsMap& toolsMap);
    void createTimers();
    SessionPlan planFor(bool reseed) const;
    void spawn(const SessionPlan& plan);
    void connectProcess(QProcess* process);
    void answerTools(const Messages& messages);

    void onOutput(QProcess* process);
    void onFinished(QProcess* process);
    void onStartFailed(QProcess* process);
    void onIdleTimeout();
    void onHardTimeout();

    void dispatch(const std::vector<StreamEvent>& events);
    void handle(const StreamEvent& event);
    void handleMcpRequest(const StreamEvent& event);
    void handleSessionMissing(const StreamEvent& event);
    void flushToolCalls();

    void conclude(Completion completion, Closing closing);
    void deliver(Completion completion);
    void write(const QByteArray& data);
    void restartIdleTimer();
    void retireProcess();

    ClaudeLauncher m_launcher;
    ClaudeSession m_session;
    Request m_request;
    StreamJsonParser m_parser;
    std::optional<McpDispatcher> m_dispatcher;

    Stage m_stage { Stage::Idle };
    SessionMode m_mode { SessionMode::Ephemeral };
    bool m_down { false };
    QString m_sessionId;

    CompletionCreatedCallback m_callback;
    PartialTextCallback m_partial;
    std::optional<Completion> m_outcome;

    std::vector<ToolRequest> m_queued;
    std::vector<ToolRequest> m_awaiting;

    QProcess* m_process { nullptr };
    QTimer* m_idleTimer { nullptr };
    QTimer* m_hardTimer { nullptr };
};

ClaudeStdioClient::Run::Run(ClaudeLauncher launcher, ClaudeSession session)
    : m_launcher(std::move(launcher))
    , m_session(std::move(session))
{
    if (m_session.keep && claudecli::isSessionId(m_session.resume))
        m_sessionId = m_session.resume;
}

ClaudeStdioClient::Run::~Run() { shutdown(); }

QString ClaudeStdioClient::Run::sessionId() const
{
    return m_session.keep ? m_sessionId : QString();
}

void ClaudeStdioClient::Run::ask(const ModelConfig& config, const Messages& messages,
                                 const ToolsMap& toolsMap,
                                 const CompletionCreatedCallback& callback,
                                 const PartialTextCallback& partialText)
{
    if (m_stage == Stage::Running)
    {
        Completion completion = claudecli::failure(claudecli::kServerError);
        callback(completion);
        return;
    }

    m_callback = callback;
    m_partial = partialText;

    switch (m_stage)
    {
        case Stage::Idle:
            begin(config, messages, toolsMap);
            break;
        case Stage::AwaitingTools:
            answerTools(messages);
            break;
        case Stage::Finished:
        {
            Completion completion
                = m_outcome ? std::move(*m_outcome) : claudecli::failure(claudecli::kServerError);
            m_outcome.reset();
            deliver(std::move(completion));
            break;
        }
        case Stage::Running:
            break;
    }
}

void ClaudeStdioClient::Run::shutdown()
{
    if (m_down)
        return;

    m_down = true;
    m_callback = nullptr;
    m_partial = nullptr;

    for (QTimer* timer : { m_idleTimer, m_hardTimer })
    {
        if (!timer)
            continue;
        timer->stop();
        timer->deleteLater();
    }
    m_idleTimer = nullptr;
    m_hardTimer = nullptr;

    retireProcess();
}

void ClaudeStdioClient::Run::begin(const ModelConfig& config, const Messages& messages,
                                   const ToolsMap& toolsMap)
{
    m_request = Request { config, messages, toolsMap };
    m_dispatcher.emplace(toolsMap);
    m_stage = Stage::Running;

    if (m_launcher.program.isEmpty())
    {
        conclude(claudecli::failure(claudecli::kNetworkError), Closing::Graceful);
        return;
    }

    createTimers();
    spawn(planFor(false));
}

void ClaudeStdioClient::Run::createTimers()
{
    const std::weak_ptr<Run> weak = weak_from_this();

    m_idleTimer = new QTimer;
    m_idleTimer->setSingleShot(true);
    QObject::connect(m_idleTimer, &QTimer::timeout, m_idleTimer,
                     [weak]
                     {
                         if (const auto run = weak.lock())
                             run->onIdleTimeout();
                     });

    m_hardTimer = new QTimer;
    m_hardTimer->setSingleShot(true);
    QObject::connect(m_hardTimer, &QTimer::timeout, m_hardTimer,
                     [weak]
                     {
                         if (const auto run = weak.lock())
                             run->onHardTimeout();
                     });
    if (m_launcher.hardTimeoutMs > 0)
        m_hardTimer->start(m_launcher.hardTimeoutMs);
}

SessionPlan ClaudeStdioClient::Run::planFor(bool reseed) const
{
    if (!m_session.keep)
        return { SessionMode::Ephemeral, QString() };

    if (!reseed && claudecli::isSessionId(m_session.resume))
        return { SessionMode::Resume, m_session.resume };

    return { SessionMode::Seed, QUuid::createUuid().toString(QUuid::WithoutBraces) };
}

void ClaudeStdioClient::Run::spawn(const SessionPlan& plan)
{
    m_parser = StreamJsonParser();
    m_queued.clear();
    m_mode = plan.mode;
    m_sessionId = plan.mode == SessionMode::Resume ? plan.id : QString();

    auto* process = new QProcess;
    m_process = process;
    process->setProgram(m_launcher.program);
    process->setArguments(commandLine(m_launcher, m_request, plan));
    process->setProcessEnvironment(m_launcher.environment);
    if (!m_launcher.workingDirectory.isEmpty())
        process->setWorkingDirectory(m_launcher.workingDirectory);
    process->setStandardErrorFile(QProcess::nullDevice());
    leaveTheApplication(*process);
    connectProcess(process);

    process->start();
    if (m_process != process || m_stage != Stage::Running)
        return;

    write(StreamJsonWriter::initialize());
    write(StreamJsonWriter::userMessage(transcriptFor(m_request, plan.mode)));
    restartIdleTimer();
}

void ClaudeStdioClient::Run::connectProcess(QProcess* process)
{
    const std::weak_ptr<Run> weak = weak_from_this();

    QObject::connect(process, &QProcess::readyReadStandardOutput, process,
                     [weak, process]
                     {
                         if (const auto run = weak.lock())
                             run->onOutput(process);
                     });
    QObject::connect(process, &QProcess::finished, process,
                     [weak, process](int, QProcess::ExitStatus)
                     {
                         if (const auto run = weak.lock())
                             run->onFinished(process);
                     });
    QObject::connect(process, &QProcess::errorOccurred, process,
                     [weak, process](QProcess::ProcessError error)
                     {
                         if (error != QProcess::FailedToStart)
                             return;
                         if (const auto run = weak.lock())
                             run->onStartFailed(process);
                     });
}

void ClaudeStdioClient::Run::answerTools(const Messages& messages)
{
    QHash<QString, Message> results;
    for (const Message& message : messages)
    {
        if (message.role == kToolRole)
            results.insert(message.tool_call_id, message);
    }

    for (const ToolRequest& request : m_awaiting)
    {
        if (!results.contains(request.call.id))
        {
            conclude(claudecli::failure(claudecli::kServerError), Closing::Kill);
            return;
        }
    }

    for (const ToolRequest& request : m_awaiting)
    {
        const Message result = results.value(request.call.id);
        write(StreamJsonWriter::mcpResponse(
            request.call.id,
            McpDispatcher::toolResponse(request.rpcId, result.content, false, result.images)));
    }

    m_awaiting.clear();
    m_stage = Stage::Running;
    restartIdleTimer();
    flushToolCalls();
}

void ClaudeStdioClient::Run::onOutput(QProcess* process)
{
    if (m_down || process != m_process)
        return;

    const QByteArray chunk = process->readAllStandardOutput();
    if (m_stage == Stage::Running)
        restartIdleTimer();
    dispatch(m_parser.feed(chunk));
}

void ClaudeStdioClient::Run::onFinished(QProcess* process)
{
    if (m_down || process != m_process)
        return;

    if (m_stage != Stage::Finished)
    {
        dispatch(m_parser.feed(process->readAllStandardOutput()));
        if (m_down || process != m_process)
            return;

        if (m_stage != Stage::Finished)
            dispatch(m_parser.endOfStream());
        if (m_down || process != m_process)
            return;
    }

    stopTimer(m_idleTimer);
    stopTimer(m_hardTimer);
}

void ClaudeStdioClient::Run::onStartFailed(QProcess* process)
{
    if (m_down || process != m_process || m_stage != Stage::Running)
        return;

    conclude(claudecli::failure(claudecli::kNetworkError), Closing::Kill);
}

void ClaudeStdioClient::Run::onIdleTimeout()
{
    if (m_down || m_stage != Stage::Running)
        return;

    conclude(claudecli::failure(claudecli::kTimeout), Closing::Kill);
}

void ClaudeStdioClient::Run::onHardTimeout()
{
    if (m_down)
        return;

    if (m_stage == Stage::Finished)
    {
        killProcessGroup(m_process);
        return;
    }

    conclude(claudecli::failure(claudecli::kTimeout), Closing::Kill);
}

void ClaudeStdioClient::Run::dispatch(const std::vector<StreamEvent>& events)
{
    for (const StreamEvent& event : events)
    {
        if (m_down)
            return;
        handle(event);
    }

    if (!m_down)
        flushToolCalls();
}

void ClaudeStdioClient::Run::handle(const StreamEvent& event)
{
    switch (event.kind)
    {
        case StreamEvent::Kind::SessionStarted:
            m_sessionId = event.text;
            break;
        case StreamEvent::Kind::PartialText:
            if (m_stage == Stage::Running && m_partial)
            {
                const PartialTextCallback partial = m_partial;
                partial(event.text);
            }
            break;
        case StreamEvent::Kind::McpRequest:
            handleMcpRequest(event);
            break;
        case StreamEvent::Kind::Answer:
            conclude(answerCompletion(event.text), Closing::Graceful);
            break;
        case StreamEvent::Kind::Failure:
            qCWarning(ClientLogic) << "claude run failed:" << event.text << event.detail;
            conclude(claudecli::failure(event.text), Closing::Graceful);
            break;
        case StreamEvent::Kind::SessionMissing:
            handleSessionMissing(event);
            break;
    }
}

void ClaudeStdioClient::Run::handleMcpRequest(const StreamEvent& event)
{
    const McpAction action = m_dispatcher->handle(event.requestId, event.message);
    if (action.kind == McpAction::Kind::Reply)
    {
        write(StreamJsonWriter::mcpResponse(event.requestId, action.reply));
        return;
    }

    m_queued.push_back({ action.toolCall, action.rpcId });
}

void ClaudeStdioClient::Run::handleSessionMissing(const StreamEvent& event)
{
    qCWarning(ClientLogic) << "claude session is missing:" << event.detail;

    if (m_mode != SessionMode::Resume)
    {
        conclude(claudecli::failure(claudecli::kServerError), Closing::Kill);
        return;
    }

    retireProcess();
    spawn(planFor(true));
}

void ClaudeStdioClient::Run::flushToolCalls()
{
    if (m_stage != Stage::Running || m_queued.empty())
        return;

    const Completion completion = toolCallsCompletion(m_queued);
    m_awaiting = std::move(m_queued);
    m_queued.clear();
    m_stage = Stage::AwaitingTools;
    stopTimer(m_idleTimer);
    deliver(completion);
}

void ClaudeStdioClient::Run::conclude(Completion completion, Closing closing)
{
    m_stage = Stage::Finished;
    stopTimer(m_idleTimer);

    if (closing == Closing::Kill)
        killProcessGroup(m_process);
    else if (m_process && m_process->state() != QProcess::NotRunning)
        m_process->closeWriteChannel();

    deliver(std::move(completion));
}

void ClaudeStdioClient::Run::deliver(Completion completion)
{
    if (!m_callback)
    {
        m_outcome = std::move(completion);
        return;
    }

    const CompletionCreatedCallback callback = std::exchange(m_callback, nullptr);
    m_partial = nullptr;
    callback(completion);
}

void ClaudeStdioClient::Run::write(const QByteArray& data)
{
    if (m_process)
        m_process->write(data);
}

void ClaudeStdioClient::Run::restartIdleTimer()
{
    if (m_idleTimer && m_launcher.idleTimeoutMs > 0)
        m_idleTimer->start(m_launcher.idleTimeoutMs);
}

void ClaudeStdioClient::Run::retireProcess()
{
    QProcess* process = std::exchange(m_process, nullptr);
    if (!process)
        return;

    killProcessGroup(process);
    process->disconnect();
    if (process->state() == QProcess::NotRunning)
    {
        process->deleteLater();
        return;
    }

    QObject::connect(process, &QProcess::finished, process, &QObject::deleteLater);
    QObject::connect(process, &QProcess::errorOccurred, process, &QObject::deleteLater);
}

ClaudeStdioClient::ClaudeStdioClient(ClaudeLauncher launcher, ClaudeSession session)
    : Client(QString(), QStringLiteral("claude-stdio"))
    , m_run(std::make_shared<Run>(std::move(launcher), std::move(session)))
{
}

ClaudeStdioClient::~ClaudeStdioClient() { m_run->shutdown(); }

void ClaudeStdioClient::createCompletionAsync(const ModelConfig& config, const Messages& messages,
                                              const ToolsMap& toolsMap,
                                              const CompletionCreatedCallback& callback) const
{
    createCompletionAsync(config, messages, toolsMap, callback, PartialTextCallback());
}

void ClaudeStdioClient::createCompletionAsync(const ModelConfig& config, const Messages& messages,
                                              const ToolsMap& toolsMap,
                                              const CompletionCreatedCallback& callback,
                                              const PartialTextCallback& partialText) const
{
    const std::shared_ptr<Run> run = m_run;
    run->ask(config, messages, toolsMap, callback, partialText);
}

Completion ClaudeStdioClient::createCompletion(const ModelConfig& config, const Messages& messages,
                                               const ToolsMap& toolsMap) const
{
    Completion result {};
    bool done = false;
    QEventLoop loop;

    createCompletionAsync(config, messages, toolsMap,
                          [&result, &done, &loop](Completion& completion)
                          {
                              result = completion;
                              done = true;
                              loop.quit();
                          });
    if (!done)
        loop.exec();
    return result;
}

QString ClaudeStdioClient::session() const { return m_run->sessionId(); }

bool ClaudeStdioClient::isAvailable() { return !claudecli::executable().isEmpty(); }

void ClaudeStdioClient::forget(const ClaudeLauncher& launcher, const QString& id)
{
    const QString path = claudecli::sessionFile(launcher, id);
    if (!path.isEmpty())
        QFile::remove(path);
}
