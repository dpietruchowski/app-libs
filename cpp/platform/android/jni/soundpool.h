#pragma once

#include <QJniObject>
#include <QString>

namespace android
{

class SoundPool final
{
public:
    static SoundPool create(int maxStreams);

    bool isValid() const;

    int load(const QString& filePath) const;
    bool play(int soundId, float leftVolume, float rightVolume, float rate) const;
    void release();

private:
    explicit SoundPool(QJniObject jni);

    QJniObject m_pool;
};

}  // namespace android
