#include "diagramlayout.h"

#include "diagram/activitylayout.h"
#include "diagram/sequencelayout.h"

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
}

Scene DiagramLayout::layout(const Diagram& diagram, const DiagramStyle& style, qreal availableWidth,
                            const TextMeasurer& measurer)
{
    return std::visit([&](const auto& typed)
                      { return layoutDiagram(typed, style, availableWidth, measurer); }, diagram);
}
}
