#pragma once

#include "async/result.h"
#include "diagram/classdiagram.h"

namespace diagram
{
class ClassParser
{
public:
    static Result<ClassDiagram> parse(const QString& text);
};
}
