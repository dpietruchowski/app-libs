#pragma once

#include "diagram/diagram.h"
#include "diagram/diagramstyle.h"
#include "diagram/scene.h"
#include "diagram/textmeasurer.h"

namespace diagram
{
class DiagramLayout
{
public:
    static Scene layout(const Diagram& diagram, const DiagramStyle& style, qreal availableWidth,
                        const TextMeasurer& measurer);
};
}
