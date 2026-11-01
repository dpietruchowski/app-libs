#pragma once

#include <QPointer>
#include <QString>
#include <QStringList>
#include <memory>
#include <vector>

#include "chat/application/agentbackend.h"

namespace chat_test
{

class FakeTurn : public chat::AgentTurn
{
public:
    void cancel() override { ++cancelCalls; }

    void reportSession(const QString& backendSession) { emit sessionReady(backendSession); }
    void reportStepStarted(const chat::Step& step) { emit stepStarted(step); }
    void reportStepFinished(const chat::Step& step) { emit stepFinished(step); }
    void complete(const QString& answer) { emit finished(answer); }
    void fail(const QString& errorToken) { emit failed(errorToken); }

    int cancelCalls { 0 };
};

class FakeBackend : public chat::AgentBackend
{
public:
    bool available() const override { return true; }

    std::unique_ptr<chat::AgentTurn> start(const chat::TurnRequest& request,
                                           const chat::AgentCatalog&) override
    {
        requests.push_back(request);
        if (refuseToStart)
        {
            return nullptr;
        }
        auto turn = std::make_unique<FakeTurn>();
        latest = turn.get();
        return turn;
    }

    void forget(const QString& backendSession) override { forgotten.append(backendSession); }

    std::vector<chat::TurnRequest> requests;
    QStringList forgotten;
    QPointer<FakeTurn> latest;
    bool refuseToStart { false };
};

}
