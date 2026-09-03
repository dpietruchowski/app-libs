#pragma once

#include "platform/haptics.h"

class HapticsAndroid final
{
public:
    static bool isAvailable();
    static void play(Haptics::Effect effect);
};
