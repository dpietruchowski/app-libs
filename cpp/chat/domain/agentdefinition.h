#pragma once

#include <QString>
#include <algorithm>
#include <vector>

#include "chat/domain/toolspec.h"

namespace chat
{

struct AgentDefinition final
{
    QString key;
    QString title;
    QString description;
    QString instructions;
    QString model;
    std::vector<ToolSpec> tools;

    const ToolSpec* findTool(const QString& toolName) const
    {
        const auto found
            = std::find_if(tools.begin(), tools.end(),
                           [&toolName](const ToolSpec& tool) { return tool.name() == toolName; });
        return found == tools.end() ? nullptr : &*found;
    }
};

}
