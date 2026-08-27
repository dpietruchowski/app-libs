#pragma once

// Short tactile feedback for user-facing milestones.
//
// Android is backed by the system Vibrator service: `Reward` plays a ramping
// three-pulse waveform at full amplitude (a celebration you cannot miss) and
// `LevelUp` a shorter, softer two-pulse variant of it, the remaining effects
// use predefined haptic primitives where the device supports them and a
// one-shot pulse otherwise. Platforms without a backend do nothing.
class Haptics final
{
public:
    enum class Effect
    {
        Tick,
        Click,
        DoubleClick,
        HeavyClick,
        LevelUp,
        Reward,
    };

    static bool isAvailable();
    static void play(Effect effect);
};
