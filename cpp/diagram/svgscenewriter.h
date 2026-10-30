#pragma once

#include "diagram/diagrampalette.h"
#include "diagram/scene.h"

#include <QString>

namespace diagram
{
class SvgSceneWriter
{
public:
    static QString write(const Scene& scene, const DiagramPalette& palette, qreal pixelRatio = 1);
};
}
