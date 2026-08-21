#pragma once

// Short tactile feedback for user-facing milestones.
//
// Android is backed by the system Vibrator service (predefined haptic effects
// where the device supports them, a one-shot pulse otherwise). Platforms
// without a backend silently do nothing.
class Haptics final
{
public:
    enum class Effect
    {
        Tick,
        Click,
        DoubleClick,
        HeavyClick,
    };

    static bool isAvailable();
    static void play(Effect effect);
};
