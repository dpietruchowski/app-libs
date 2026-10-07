#pragma once

#include "diagram/diagramstyle.h"
#include "diagram/scene.h"
#include "diagram/sequencediagram.h"
#include "diagram/textmeasurer.h"

namespace diagram
{
class SequenceLayout
{
public:
    static Scene layout(const SequenceDiagram& diagram, const DiagramStyle& style,
                        qreal availableWidth, const TextMeasurer& measurer);
};
}
