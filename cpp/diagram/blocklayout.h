#pragma once

#include "diagram/blockdiagram.h"
#include "diagram/diagramstyle.h"
#include "diagram/scene.h"
#include "diagram/textmeasurer.h"

namespace diagram
{
class BlockLayout
{
public:
    static Scene layout(const BlockDiagram& diagram, const DiagramStyle& style,
                        qreal availableWidth, const TextMeasurer& measurer);
};
}
