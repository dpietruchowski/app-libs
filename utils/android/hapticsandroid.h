#pragma once

#include "utils/haptics.h"

class HapticsAndroid final
{
public:
    static bool isAvailable();
    static void play(Haptics::Effect effect);
};
