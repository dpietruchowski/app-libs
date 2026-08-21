#pragma once

#include <QJniObject>

namespace android
{

class Vibrator final
{
public:
    static Vibrator defaultVibrator();

    bool isValid() const;
    bool hasVibrator() const;

    bool vibratePredefined(int effectId) const;
    void vibrateOneShot(qint64 milliseconds, int amplitude) const;

private:
    explicit Vibrator(QJniObject jni);

    bool vibrateEffect(const QJniObject& effect) const;

    QJniObject m_vibrator;
};

}  // namespace android
