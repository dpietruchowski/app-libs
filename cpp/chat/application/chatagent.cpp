#include "chat/application/chatagent.h"

#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaObject>
#include <QUuid>
#include <QVariant>
#include <QVariantMap>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "agent/agent.h"
#include "chat/application/agentcatalog.h"
#include "chat/application/agentturn.h"
#include "chat/domain/enumnames.h"

namespace chat
{

namespace
{

    const QString kTaskParameter = QStringLiteral("task");
    constexpr int kMaxFollowUps = 2;

    QString withContext(const QString& text, const std::optional<ChatContext>& context)
    {
        if (!context)
        {
            return text;
        }
        const QString data
            = QString::fromUtf8(QJsonDocument(context->data).toJson(QJsonDocument::Compact));
        return text + QStringLiteral("\n[context: %1 %2]").arg(context->title, data);
    }

    QString delegationDocstring(const AppAgent& entry)
    {
        return QStringLiteral("@brief %1\n@details %2\n@param %3 (string) A precise, "
                              "self-contained task; the agent does not see this conversation.")
            .arg(entry.key, entry.description.simplified(), kTaskParameter);
    }

    QString failureReport(const AppAgent& entry, const QString& errorToken)
    {
        return QStringLiteral("The %1 agent failed (%2).").arg(entry.title, errorToken);
    }

    Step openStep(StepKind kind, const QString& agent, const QString& tool, const ToolCall& call,
                  ToolEffect effect, const std::optional<QUuid>& parentId)
    {
        Step step;
        step.id = QUuid::createUuid();
        step.parentId = parentId;
        step.kind = kind;
        step.agent = agent;
        step.tool = tool;
        step.input = QJsonObject::fromVariantMap(call.arguments);
        step.effect = effect;
        return step;
    }

    Step closedStep(Step step, StepStatus status, const QJsonValue& output = QJsonValue())
    {
        step.status = status;
        step.output = output;
        return step;
    }

    struct DelegateRun final
    {
        std::unique_ptr<::Client> client;
        std::unique_ptr<::Agent> agent;
        int followUps { 0 };
    };

    QString nextFollowUp(const AppAgent& entry, DelegateRun& run)
    {
        if (!entry.followUp || run.followUps >= kMaxFollowUps)
        {
            return QString();
        }
        const QString message = entry.followUp();
        if (!message.isEmpty())
        {
            ++run.followUps;
        }
        return message;
    }

    class ChatTurn final : public AgentTurn
    {
    public:
        ChatTurn(const TurnRequest& request, std::vector<AppAgent> agents, ClientFactory clients,
                 std::unique_ptr<::Client> client)
            : m_agents(std::move(agents))
            , m_clients(std::move(clients))
            , m_context(request.context)
            , m_model(request.model)
            , m_reasoningEffort(request.reasoningEffort)
            , m_agentModels(request.agentModels)
            , m_question(withContext(request.transcript.back().text(), m_context))
            , m_client(std::move(client))
            , m_agent(request.model, request.mainInstructions)
        {
            m_agent.setReasoningEffort(request.reasoningEffort);
            for (auto message = request.transcript.begin(); message != request.transcript.end() - 1;
                 ++message)
            {
                m_agent.addMessage(roleName(message->role()), message->text());
            }
            for (const AppAgent& entry : m_agents)
            {
                m_agent.addTool(delegationDocstring(entry),
                                [this, &entry](QVariantMap arguments, ToolDone done)
                                { delegate(entry, arguments, std::move(done)); });
            }
            m_agent.setObserver(mainObserver());
            m_agent.setPartialTextCallback([this](const QString& text) { emit partialText(text); });

            QMetaObject::invokeMethod(this, [this] { ask(); }, Qt::QueuedConnection);
        }

        void cancel() override
        {
            if (m_concluded)
            {
                return;
            }
            m_agent.cancel();
            if (m_running)
            {
                m_running->agent->cancel();
                retireRun();
            }
            m_retired.push_back(std::shared_ptr<void>(std::move(m_client)));
            fail(turn_error::kCancelled);
        }

    private:
        void ask()
        {
            if (m_concluded)
            {
                return;
            }
            m_agent.requestAsync(*m_client, m_question, [](const QString&) {});
        }

        AgentObserver mainObserver()
        {
            AgentObserver observer;
            observer.toolStarted = [this](const ToolCall& call, ToolEffect effect)
            {
                m_delegation
                    = openStep(StepKind::Delegation, call.name, QString(), call, effect, {});
                emit stepStarted(*m_delegation);
            };
            observer.toolFinished = [this](const ToolCall&, const QVariant& result)
            {
                if (m_delegation)
                {
                    finishStep(m_delegation, StepStatus::Ok, QJsonValue::fromVariant(result));
                }
            };
            observer.finished = [this](const QString& answer) { succeed(answer); };
            observer.failed = [this](const QString& errorToken) { fail(errorToken); };
            return observer;
        }

        AgentObserver delegateObserver(const AppAgent& entry, DelegateRun& run, ToolDone done)
        {
            AgentObserver observer;
            observer.toolStarted = [this, &entry](const ToolCall& call, ToolEffect effect)
            {
                m_tool = openStep(StepKind::Tool, entry.key, call.name, call, effect,
                                  m_delegation ? std::optional<QUuid>(m_delegation->id)
                                               : std::nullopt);
                emit stepStarted(*m_tool);
            };
            observer.toolFinished = [this](const ToolCall&, const QVariant& result)
            {
                if (m_tool)
                {
                    finishStep(m_tool, StepStatus::Ok, QJsonValue::fromVariant(result));
                }
            };
            observer.finished = [this, &entry, &run, done](const QString& report)
            {
                const QString message = nextFollowUp(entry, run);
                if (!message.isEmpty())
                {
                    if (std::unique_ptr<::Client> next = m_clients(ClaudeSession {}))
                    {
                        m_retired.push_back(std::shared_ptr<void>(std::move(run.client)));
                        run.client = std::move(next);
                        run.agent->requestAsync(*run.client, message, [](const QString&) {});
                        return;
                    }
                }
                retireRun();
                done(report);
            };
            observer.failed = [this, &entry, done](const QString& errorToken)
            {
                retireRun();
                done(failureReport(entry, errorToken));
            };
            return observer;
        }

        void delegate(const AppAgent& entry, const QVariantMap& arguments, ToolDone done)
        {
            auto run = std::make_shared<DelegateRun>();
            run->client = m_clients(ClaudeSession {});
            if (!run->client)
            {
                done(failureReport(entry, turn_error::kUnavailable));
                return;
            }
            run->agent = std::make_unique<::Agent>(entry.agent);
            run->agent->clear();
            run->agent->setModel(entry.modelFor(m_agentModels.value(entry.key), m_model));
            run->agent->setReasoningEffort(entry.config.reasoningEffort.isEmpty()
                                               ? m_reasoningEffort
                                               : entry.config.reasoningEffort);
            run->agent->setObserver(delegateObserver(entry, *run, std::move(done)));
            m_running = run;
            run->agent->requestAsync(
                *run->client, withContext(arguments.value(kTaskParameter).toString(), m_context),
                [](const QString&) {});
        }

        void retireRun()
        {
            if (m_running)
            {
                m_retired.push_back(std::move(m_running));
            }
        }

        void finishStep(std::optional<Step>& open, StepStatus status, const QJsonValue& output)
        {
            const Step step = closedStep(*open, status, output);
            open.reset();
            emit stepFinished(step);
        }

        void succeed(const QString& answer)
        {
            if (m_concluded)
            {
                return;
            }
            m_concluded = true;
            const QString session = m_client->session();
            if (!session.isEmpty())
            {
                emit sessionReady(session);
            }
            emit finished(answer);
        }

        void fail(const QString& errorToken)
        {
            if (m_concluded)
            {
                return;
            }
            m_concluded = true;
            if (m_tool)
            {
                finishStep(m_tool, StepStatus::Failed, QJsonValue());
            }
            if (m_delegation)
            {
                finishStep(m_delegation, StepStatus::Failed, QJsonValue());
            }
            emit failed(errorToken);
        }

        std::vector<AppAgent> m_agents;
        ClientFactory m_clients;
        std::optional<ChatContext> m_context;
        QString m_model;
        QString m_reasoningEffort;
        QHash<QString, QString> m_agentModels;
        QString m_question;
        std::unique_ptr<::Client> m_client;
        ::Agent m_agent;
        std::optional<Step> m_delegation;
        std::optional<Step> m_tool;
        std::shared_ptr<DelegateRun> m_running;
        std::vector<std::shared_ptr<void>> m_retired;
        bool m_concluded { false };
    };

}

ChatAgent::ChatAgent(ClientFactory clients, std::function<bool()> available,
                     std::function<void(const QString&)> forget)
    : m_clients(std::move(clients))
    , m_available(std::move(available))
    , m_forget(std::move(forget))
{
}

bool ChatAgent::available() const { return m_available && m_available(); }

std::unique_ptr<AgentTurn> ChatAgent::start(const TurnRequest& request, const AgentCatalog& catalog)
{
    if (!available() || request.transcript.empty())
    {
        return nullptr;
    }
    std::unique_ptr<::Client> client
        = m_clients(ClaudeSession { .resume = request.backendSession, .keep = true });
    if (!client)
    {
        return nullptr;
    }
    return std::make_unique<ChatTurn>(request, catalog.agents(), m_clients, std::move(client));
}

void ChatAgent::forget(const QString& backendSession)
{
    if (m_forget)
    {
        m_forget(backendSession);
    }
}

}
