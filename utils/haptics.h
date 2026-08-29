#pragma once

// Short tactile feedback for user-facing milestones.
//
// Android is backed by the system Vibrator service: `Mastery` plays a rising
// four-pulse waveform that lands on a full-amplitude beat and fades out on a
// soft tail (the strongest effect, celebratory rather than urgent), `Reward` a
// ramping three-pulse waveform at full amplitude and `LevelUp` a shorter,
// softer two-pulse variant of it, the remaining effects use predefined haptic
// primitives where the device supports them and a one-shot pulse otherwise.
// Platforms without a backend do nothing.
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
        Mastery,
    };

    static bool isAvailable();
    static void play(Effect effect);
};
