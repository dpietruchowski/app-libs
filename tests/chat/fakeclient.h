#pragma once

#include <QString>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "../agent/completions.h"
#include "agent/claudesession.h"
#include "agent/client.h"
#include "agent/structures.h"

namespace chat_test
{

using agent_test::contentCompletion;
using agent_test::failedCompletion;
using agent_test::toolCall;
using agent_test::toolCallCompletion;
using agent_test::toolCallsCompletion;

struct ClientRequest
{
    ModelConfig config;
    ::Messages messages;
    ToolsMap tools;
    Client::CompletionCreatedCallback callback;
    Client::PartialTextCallback partialText;
};

struct ClientState
{
    ClaudeSession requested;
    QString session;
    std::optional<Completion> immediate;
    std::vector<ClientRequest> held;
    bool destroyed { false };

    void completeNext(Completion completion)
    {
        const ClientRequest next = held.front();
        held.erase(held.begin());
        next.callback(completion);
    }

    void streamNext(const QString& textSoFar) const { held.front().partialText(textSoFar); }
};

class ScriptedClient : public ::Client
{
public:
    explicit ScriptedClient(std::shared_ptr<ClientState> state)
        : Client("http://mock.invalid", "test-key")
        , m_state(std::move(state))
    {
    }

    ~ScriptedClient() override { m_state->destroyed = true; }

    using Client::createCompletionAsync;

    void createCompletionAsync(const ModelConfig& config, const ::Messages& messages,
                               const ToolsMap& toolsMap, const CompletionCreatedCallback& callback,
                               const PartialTextCallback& partialText) const override
    {
        if (m_state->immediate)
        {
            Completion completion = *m_state->immediate;
            callback(completion);
            return;
        }
        m_state->held.push_back({ config, messages, toolsMap, callback, partialText });
    }

    QString session() const override { return m_state->session; }

private:
    std::shared_ptr<ClientState> m_state;
};

}
