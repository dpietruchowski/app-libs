#pragma once

#include "async/result.h"
#include "diagram/activitydiagram.h"

namespace diagram
{
class ActivityParser
{
public:
    static Result<ActivityDiagram> parse(const QString& text);
};
}
