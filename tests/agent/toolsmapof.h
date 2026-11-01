#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include "agent/structures.h"

inline ToolData toolData(const QString& name, const QString& description)
{
    const QJsonObject parameters { { "type", "object" },
                                   { "properties", QJsonObject() },
                                   { "additionalProperties", false } };
    const QJsonObject function { { "strict", true },
                                 { "name", name },
                                 { "description", description },
                                 { "parameters", parameters } };
    return ToolData { .tool = nullptr,
                      .json = QJsonObject { { "type", "function" }, { "function", function } } };
}

inline ToolsMap toolsMapOf(const QStringList& names)
{
    ToolsMap toolsMap;
    for (const QString& name : names)
        toolsMap.insert(name, toolData(name, "Does " + name + "."));
    return toolsMap;
}
