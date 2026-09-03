#include "vibrator.h"
#include "context.h"

#include <QCoreApplication>
#include <QJniEnvironment>
#include <QVarLengthArray>

namespace
{

constexpr int VibratorManagerSdk = 31;
constexpr int PredefinedEffectSdk = 29;
constexpr int VibrationEffectSdk = 26;
constexpr int NoRepeat = -1;

int sdkVersion() { return QNativeInterface::QAndroidApplication::sdkVersion(); }

}  // namespace

namespace android
{

Vibrator Vibrator::defaultVibrator()
{
    if (sdkVersion() >= VibratorManagerSdk)
    {
        QJniObject manager = Context::application().systemService("vibrator_manager");
        if (manager.isValid())
            return Vibrator(
                manager.callObjectMethod("getDefaultVibrator", "()Landroid/os/Vibrator;"));
    }
    return Vibrator(Context::application().systemService("vibrator"));
}

Vibrator::Vibrator(QJniObject jni)
    : m_vibrator(std::move(jni))
{
}

bool Vibrator::isValid() const { return m_vibrator.isValid(); }

bool Vibrator::hasVibrator() const
{
    return m_vibrator.isValid() && m_vibrator.callMethod<jboolean>("hasVibrator");
}

bool Vibrator::hasAmplitudeControl() const
{
    return m_vibrator.isValid() && sdkVersion() >= VibrationEffectSdk
        && m_vibrator.callMethod<jboolean>("hasAmplitudeControl");
}

bool Vibrator::vibratePredefined(int effectId) const
{
    if (sdkVersion() < PredefinedEffectSdk)
        return false;

    QJniObject effect = QJniObject::callStaticObjectMethod(
        "android/os/VibrationEffect", "createPredefined", "(I)Landroid/os/VibrationEffect;",
        static_cast<jint>(effectId));
    return vibrateEffect(effect);
}

bool Vibrator::vibrateWaveform(const QList<qint64>& timings, const QList<int>& amplitudes) const
{
    if (!m_vibrator.isValid() || timings.isEmpty())
        return false;

    QJniEnvironment env;
    QVarLengthArray<jlong> timingValues(timings.size());
    std::copy(timings.begin(), timings.end(), timingValues.begin());

    jlongArray timingArray = env->NewLongArray(timings.size());
    if (timingArray == nullptr)
        return false;
    env->SetLongArrayRegion(timingArray, 0, timings.size(), timingValues.data());

    bool played = false;
    if (sdkVersion() < VibrationEffectSdk)
    {
        m_vibrator.callMethod<void>("vibrate", "([JI)V", timingArray, static_cast<jint>(NoRepeat));
        played = true;
    }
    else if (hasAmplitudeControl() && amplitudes.size() == timings.size())
    {
        QVarLengthArray<jint> amplitudeValues(amplitudes.size());
        std::copy(amplitudes.begin(), amplitudes.end(), amplitudeValues.begin());

        jintArray amplitudeArray = env->NewIntArray(amplitudes.size());
        if (amplitudeArray != nullptr)
        {
            env->SetIntArrayRegion(amplitudeArray, 0, amplitudes.size(), amplitudeValues.data());
            QJniObject effect = QJniObject::callStaticObjectMethod(
                "android/os/VibrationEffect", "createWaveform",
                "([J[II)Landroid/os/VibrationEffect;", timingArray, amplitudeArray,
                static_cast<jint>(NoRepeat));
            played = vibrateEffect(effect);
            env->DeleteLocalRef(amplitudeArray);
        }
    }
    else
    {
        QJniObject effect = QJniObject::callStaticObjectMethod(
            "android/os/VibrationEffect", "createWaveform", "([JI)Landroid/os/VibrationEffect;",
            timingArray, static_cast<jint>(NoRepeat));
        played = vibrateEffect(effect);
    }

    env->DeleteLocalRef(timingArray);
    return played;
}

void Vibrator::vibrateOneShot(qint64 milliseconds, int amplitude) const
{
    if (!m_vibrator.isValid())
        return;

    if (sdkVersion() < VibrationEffectSdk)
    {
        m_vibrator.callMethod<void>("vibrate", "(J)V", static_cast<jlong>(milliseconds));
        return;
    }

    QJniObject effect = QJniObject::callStaticObjectMethod(
        "android/os/VibrationEffect", "createOneShot", "(JI)Landroid/os/VibrationEffect;",
        static_cast<jlong>(milliseconds), static_cast<jint>(amplitude));
    vibrateEffect(effect);
}

bool Vibrator::vibrateEffect(const QJniObject& effect) const
{
    if (!m_vibrator.isValid() || !effect.isValid())
        return false;

    m_vibrator.callMethod<void>("vibrate", "(Landroid/os/VibrationEffect;)V",
                                effect.object<jobject>());
    return true;
}

}  // namespace android
