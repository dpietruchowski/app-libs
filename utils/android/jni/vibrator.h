#pragma once

#include <QJniObject>
#include <QList>

namespace android
{

class Vibrator final
{
public:
    static Vibrator defaultVibrator();

    bool isValid() const;
    bool hasVibrator() const;
    bool hasAmplitudeControl() const;

    bool vibratePredefined(int effectId) const;
    bool vibrateWaveform(const QList<qint64>& timings, const QList<int>& amplitudes) const;
    void vibrateOneShot(qint64 milliseconds, int amplitude) const;

private:
    explicit Vibrator(QJniObject jni);

    bool vibrateEffect(const QJniObject& effect) const;

    QJniObject m_vibrator;
};

}  // namespace android
