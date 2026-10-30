#pragma once

#include "async/result.h"
#include "diagram/bitsdiagram.h"

namespace diagram
{
class BitsParser
{
public:
    static Result<BitsDiagram> parse(const QString& text);
};
}
