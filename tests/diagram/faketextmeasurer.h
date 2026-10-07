#pragma once

#include "diagram/textmeasurer.h"

class FakeTextMeasurer final : public diagram::TextMeasurer
{
public:
    static constexpr qreal kCharacterWidth = 7;
    static constexpr qreal kLineHeight = 16;
    static constexpr qreal kAscent = 12;

    qreal width(const QString& text) const override { return text.size() * kCharacterWidth; }
    qreal lineHeight() const override { return kLineHeight; }
    qreal ascent() const override { return kAscent; }
};
