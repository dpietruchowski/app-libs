#include "hapticsandroid.h"

#include "jni/vibrator.h"

namespace
{

struct EffectDescription
{
    int predefinedId;
    qint64 durationMs;
    int amplitude;
};

constexpr int DefaultAmplitude = -1;

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
    if (!vibrator.vibratePredefined(description.predefinedId))
        vibrator.vibrateOneShot(description.durationMs, description.amplitude);
}
