#pragma once

#include "async/result.h"
#include "diagram/sequencediagram.h"

namespace diagram
{
class SequenceParser
{
public:
    static Result<SequenceDiagram> parse(const QString& text);
};
}
