#include "diagramlayout.h"

#include "diagram/activitylayout.h"
#include "diagram/blocklayout.h"
#include "diagram/classlayout.h"
#include "diagram/sequencelayout.h"
#include "diagram/statelayout.h"

#include <variant>

namespace diagram
{
namespace
{
    Scene layoutDiagram(const SequenceDiagram& diagram, const DiagramStyle& style,
                        qreal availableWidth, const TextMeasurer& measurer)
    {
        return SequenceLayout::layout(diagram, style, availableWidth, measurer);
    }

    Scene layoutDiagram(const ActivityDiagram& diagram, const DiagramStyle& style,
                        qreal availableWidth, const TextMeasurer& measurer)
    {
        return ActivityLayout::layout(diagram, style, availableWidth, measurer);
    }

    Scene layoutDiagram(const BlockDiagram& diagram, const DiagramStyle& style,
                        qreal availableWidth, const TextMeasurer& measurer)
    {
        return BlockLayout::layout(diagram, style, availableWidth, measurer);
    }

    Scene layoutDiagram(const StateDiagram& diagram, const DiagramStyle& style,
                        qreal availableWidth, const TextMeasurer& measurer)
    {
        return StateLayout::layout(diagram, style, availableWidth, measurer);
    }

    Scene layoutDiagram(const ClassDiagram& diagram, const DiagramStyle& style,
                        qreal availableWidth, const TextMeasurer& measurer)
    {
        return ClassLayout::layout(diagram, style, availableWidth, measurer);
    }
}

Scene DiagramLayout::layout(const Diagram& diagram, const DiagramStyle& style, qreal availableWidth,
                            const TextMeasurer& measurer)
{
    return std::visit([&](const auto& typed)
                      { return layoutDiagram(typed, style, availableWidth, measurer); }, diagram);
}
}
