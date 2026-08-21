#include "vibrator.h"
#include "context.h"

#include <QCoreApplication>

namespace
{

constexpr int VibratorManagerSdk = 31;
constexpr int PredefinedEffectSdk = 29;
constexpr int VibrationEffectSdk = 26;

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

bool Vibrator::vibratePredefined(int effectId) const
{
    if (sdkVersion() < PredefinedEffectSdk)
        return false;

    QJniObject effect = QJniObject::callStaticObjectMethod(
        "android/os/VibrationEffect", "createPredefined", "(I)Landroid/os/VibrationEffect;",
        static_cast<jint>(effectId));
    return vibrateEffect(effect);
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
