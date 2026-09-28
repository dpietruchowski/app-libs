#include "keyboardhaptics.h"

#include "platform/haptics.h"

KeyboardHaptics::KeyboardHaptics(QObject* parent)
    : QObject(parent)
{
}

void KeyboardHaptics::keyPress() const { Haptics::play(Haptics::Effect::KeyPress); }

void KeyboardHaptics::longPress() const { Haptics::play(Haptics::Effect::LongPress); }
