#pragma once

#include "diagram/classdiagram.h"
#include "diagram/diagramstyle.h"
#include "diagram/scene.h"
#include "diagram/textmeasurer.h"

namespace diagram
{
class ClassLayout
{
public:
    static Scene layout(const ClassDiagram& diagram, const DiagramStyle& style,
                        qreal availableWidth, const TextMeasurer& measurer);
};
}
