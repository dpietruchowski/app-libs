#pragma once

#include "diagram/scene.h"

#include <QColor>

namespace diagram
{
struct DiagramPalette
{
    QColor background { 0xff, 0xff, 0xff };
    QColor line { 0x44, 0x44, 0x44 };
    QColor text { 0x22, 0x22, 0x22 };
    QColor mutedText { 0x66, 0x66, 0x66 };
    QColor boxFill { 0xee, 0xf0, 0xf8 };
    QColor boxStroke { 0x55, 0x5f, 0x7a };
    QColor noteFill { 0xff, 0xf6, 0xd5 };
    QColor noteStroke { 0xb8, 0xa0, 0x60 };
    QColor groupStroke { 0x88, 0x88, 0x88 };
    QColor divider { 0x88, 0x88, 0x88 };

    QColor color(ColorRole role) const
    {
        switch (role)
        {
            case ColorRole::Background:
                return background;
            case ColorRole::Line:
                return line;
            case ColorRole::Text:
                return text;
            case ColorRole::MutedText:
                return mutedText;
            case ColorRole::BoxFill:
                return boxFill;
            case ColorRole::BoxStroke:
                return boxStroke;
            case ColorRole::NoteFill:
                return noteFill;
            case ColorRole::NoteStroke:
                return noteStroke;
            case ColorRole::GroupStroke:
                return groupStroke;
            case ColorRole::Divider:
                return divider;
        }
        return text;
    }
};
}
