#include "hapticsandroid.h"

#include "jni/vibrator.h"

#include <QList>

namespace
{

struct EffectDescription
{
    int predefinedId;
    qint64 durationMs;
    int amplitude;
};

constexpr int DefaultAmplitude = -1;
constexpr int MaxAmplitude = 255;
constexpr qint64 MasteryFallbackDurationMs = 520;
constexpr qint64 RewardFallbackDurationMs = 400;
constexpr qint64 LevelUpFallbackDurationMs = 110;
constexpr int LevelUpFallbackAmplitude = 170;
constexpr int MasteryTailAmplitude = 90;

const QList<qint64> MasteryTimings { 0, 45, 45, 55, 45, 70, 45, 200, 45, 110 };
const QList<int> MasteryAmplitudes {
    0, 110, 0, 155, 0, 200, 0, MaxAmplitude, 0, MasteryTailAmplitude
};

const QList<qint64> RewardTimings { 0, 70, 60, 90, 60, 220 };
const QList<int> RewardAmplitudes { 0, 140, 0, 200, 0, MaxAmplitude };

const QList<qint64> LevelUpTimings { 0, 40, 45, 100 };
const QList<int> LevelUpAmplitudes { 0, 120, 0, 180 };

struct Waveform
{
    const QList<qint64>* timings;
    const QList<int>* amplitudes;
};

Waveform describeWaveform(Haptics::Effect effect)
{
    switch (effect)
    {
        case Haptics::Effect::Tick:
        case Haptics::Effect::Click:
        case Haptics::Effect::DoubleClick:
        case Haptics::Effect::HeavyClick:
            return { nullptr, nullptr };
        case Haptics::Effect::LevelUp:
            return { &LevelUpTimings, &LevelUpAmplitudes };
        case Haptics::Effect::Reward:
            return { &RewardTimings, &RewardAmplitudes };
        case Haptics::Effect::Mastery:
            return { &MasteryTimings, &MasteryAmplitudes };
    }
    return { nullptr, nullptr };
}

EffectDescription describe(Haptics::Effect effect)
{
    switch (effect)
    {
        case Haptics::Effect::Tick:
            return { 2, 10, DefaultAmplitude };
        case Haptics::Effect::Click:
            return { 0, 20, DefaultAmplitude };
        case Haptics::Effect::DoubleClick:
            return { 1, 40, DefaultAmplitude };
        case Haptics::Effect::HeavyClick:
            return { 5, 60, DefaultAmplitude };
        case Haptics::Effect::LevelUp:
            return { 5, LevelUpFallbackDurationMs, LevelUpFallbackAmplitude };
        case Haptics::Effect::Reward:
            return { 5, RewardFallbackDurationMs, MaxAmplitude };
        case Haptics::Effect::Mastery:
            return { 5, MasteryFallbackDurationMs, MaxAmplitude };
    }
    return { 0, 20, DefaultAmplitude };
}

}  // namespace

bool HapticsAndroid::isAvailable() { return android::Vibrator::defaultVibrator().hasVibrator(); }

void HapticsAndroid::play(Haptics::Effect effect)
{
    const android::Vibrator vibrator = android::Vibrator::defaultVibrator();
    if (!vibrator.hasVibrator())
        return;

    const EffectDescription description = describe(effect);
    const Waveform waveform = describeWaveform(effect);
    const bool played = waveform.timings != nullptr
        ? vibrator.vibrateWaveform(*waveform.timings, *waveform.amplitudes)
        : vibrator.vibratePredefined(description.predefinedId);
    if (!played)
        vibrator.vibrateOneShot(description.durationMs, description.amplitude);
}
