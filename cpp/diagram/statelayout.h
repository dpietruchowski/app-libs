#pragma once

#include "diagram/diagramstyle.h"
#include "diagram/scene.h"
#include "diagram/statediagram.h"
#include "diagram/textmeasurer.h"

namespace diagram
{
class StateLayout
{
public:
    static Scene layout(const StateDiagram& diagram, const DiagramStyle& style,
                        qreal availableWidth, const TextMeasurer& measurer);
};
}
