#pragma once

// Short tactile feedback for user-facing milestones.
//
// Android is backed by the system Vibrator service: `Reward` plays a ramping
// multi-pulse waveform at full amplitude (a celebration you cannot miss), the
// remaining effects use predefined haptic primitives where the device supports
// them and a one-shot pulse otherwise. Platforms without a backend do nothing.
class Haptics final
{
public:
    enum class Effect
    {
        Tick,
        Click,
        DoubleClick,
        HeavyClick,
        Reward,
    };

    static bool isAvailable();
    static void play(Effect effect);
};
