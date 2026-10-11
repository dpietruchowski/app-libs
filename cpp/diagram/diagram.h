#pragma once

#include "diagram/activitydiagram.h"
#include "diagram/blockdiagram.h"
#include "diagram/classdiagram.h"
#include "diagram/sequencediagram.h"
#include "diagram/statediagram.h"

#include <QStringList>

#include <variant>

namespace diagram
{
using Diagram
    = std::variant<SequenceDiagram, ActivityDiagram, BlockDiagram, StateDiagram, ClassDiagram>;

inline const QStringList& warningsOf(const Diagram& diagram)
{
    return std::visit([](const auto& typed) -> const QStringList& { return typed.warnings; },
                      diagram);
}
}
