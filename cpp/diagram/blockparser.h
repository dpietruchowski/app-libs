#pragma once

#include "async/result.h"
#include "diagram/blockdiagram.h"

namespace diagram
{
class BlockParser
{
public:
    static Result<BlockDiagram> parse(const QString& text);
};
}
