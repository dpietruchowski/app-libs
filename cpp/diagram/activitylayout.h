#pragma once

#include "diagram/activitydiagram.h"
#include "diagram/diagramstyle.h"
#include "diagram/scene.h"
#include "diagram/textmeasurer.h"

namespace diagram
{
class ActivityLayout
{
public:
    static Scene layout(const ActivityDiagram& diagram, const DiagramStyle& style,
                        qreal availableWidth, const TextMeasurer& measurer);
};
}
