#pragma once

#include "diagram/bitsdiagram.h"
#include "diagram/diagramstyle.h"
#include "diagram/scene.h"
#include "diagram/textmeasurer.h"

namespace diagram
{
class BitsLayout
{
public:
    static Scene layout(const BitsDiagram& diagram, const DiagramStyle& style, qreal availableWidth,
                        const TextMeasurer& measurer);
};
}
