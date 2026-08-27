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
constexpr qint64 RewardFallbackDurationMs = 400;
constexpr qint64 LevelUpFallbackDurationMs = 160;
constexpr int LevelUpFallbackAmplitude = 200;

const QList<qint64> RewardTimings { 0, 70, 60, 90, 60, 220 };
const QList<int> RewardAmplitudes { 0, 140, 0, 200, 0, MaxAmplitude };

const QList<qint64> LevelUpTimings { 0, 55, 55, 150 };
const QList<int> LevelUpAmplitudes { 0, 150, 0, 215 };

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
