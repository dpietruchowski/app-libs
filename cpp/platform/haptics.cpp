#include "haptics.h"

#include <QtGlobal>

#if defined(Q_OS_ANDROID)
#include "android/hapticsandroid.h"
#define HAPTICS_BACKEND HapticsAndroid
#endif

bool Haptics::isAvailable()
{
#ifdef HAPTICS_BACKEND
    return HAPTICS_BACKEND::isAvailable();
#else
    return false;
#endif
}

void Haptics::play(Effect effect)
{
#ifdef HAPTICS_BACKEND
    HAPTICS_BACKEND::play(effect);
#else
    Q_UNUSED(effect);
#endif
}
