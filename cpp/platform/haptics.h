#pragma once

// Short tactile feedback for user-facing milestones.
//
// Android is backed by the system Vibrator service: `Mastery` plays a rising
// four-pulse waveform that lands on a full-amplitude beat and fades out on a
// soft tail (the strongest effect, celebratory rather than urgent), `Reward` a
// ramping three-pulse waveform at full amplitude, `LevelUp` a shorter,
// softer two-pulse variant of it and `Progress` half of a level-up: a single
// soft, longer pulse. A waveform the device cannot play falls back to a single
// one-shot pulse. The remaining effects use predefined haptic
// primitives where the device supports them and a one-shot pulse otherwise.
// `KeyPress` and `LongPress` go through the window's view haptic feedback, like
// the system keyboard, so they follow the user's touch feedback setting.
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
        Progress,
        LevelUp,
        Reward,
        Mastery,
        KeyPress,
        LongPress,
    };

    static bool isAvailable();
    static void play(Effect effect);
};
