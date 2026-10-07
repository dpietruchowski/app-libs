#pragma once

#include <QFont>
#include <QString>

namespace diagram
{
struct DiagramStyle
{
    QString fontFamily = QStringLiteral("sans-serif");
    int fontPixelSize = 14;
    qreal textSafety = 1.06;
    qreal margin = 4;
    qreal padding = 6;
    qreal columnGap = 8;
    qreal rowGap = 10;
    qreal minColumnWidth = 64;
    qreal minLabelWidth = 160;
    qreal labelHalo = 2;
    qreal arrowHeadSize = 8;
    qreal selfLoopWidth = 24;
    qreal groupInset = 6;
    int maxGroupIndent = 6;
    qreal boxRadius = 4;
    qreal actorHeight = 32;
    int repeatFootAfterSteps = 8;

    QFont font() const
    {
        QFont font(fontFamily);
        font.setPixelSize(fontPixelSize);
        return font;
    }
};
}
