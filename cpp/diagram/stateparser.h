#pragma once

#include "async/result.h"
#include "diagram/statediagram.h"

namespace diagram
{
class StateParser
{
public:
    static Result<StateDiagram> parse(const QString& text);
};
}
