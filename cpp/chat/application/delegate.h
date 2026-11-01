#pragma once

#include <QString>
#include <QUuid>
#include <memory>

#include "chat/application/agentturn.h"

namespace chat
{

class Delegate
{
public:
    virtual ~Delegate() = default;

    virtual const QString& key() const = 0;
    virtual const QString& title() const = 0;
    virtual const QString& description() const = 0;
    virtual std::unique_ptr<AgentTurn> delegate(const QString& task, const QUuid& delegationStepId)
        = 0;
};

}
