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

const QList<qint64> RewardTimings { 0, 70, 60, 90, 60, 220 };
const QList<int> RewardAmplitudes { 0, 140, 0, 200, 0, MaxAmplitude };

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
    const bool played = effect == Haptics::Effect::Reward
        ? vibrator.vibrateWaveform(RewardTimings, RewardAmplitudes)
        : vibrator.vibratePredefined(description.predefinedId);
    if (!played)
        vibrator.vibrateOneShot(description.durationMs, description.amplitude);
}
