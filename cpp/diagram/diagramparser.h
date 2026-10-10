#pragma once

#include "async/result.h"
#include "diagram/diagram.h"

namespace diagram
{
class DiagramParser
{
public:
    static Result<Diagram> parse(const QString& text);
};
}
