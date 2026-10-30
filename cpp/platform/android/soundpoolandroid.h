#pragma once

#include "jni/soundpool.h"

#include <QString>

class SoundPoolAndroid final
{
public:
    explicit SoundPoolAndroid(int maxStreams);
    ~SoundPoolAndroid();

    SoundPoolAndroid(const SoundPoolAndroid&) = delete;
    SoundPoolAndroid& operator=(const SoundPoolAndroid&) = delete;

    bool isAvailable() const;
    int load(const QString& path);
    bool play(int soundId, float volume, float rate);

private:
    static QString readableFile(const QString& path);

    android::SoundPool m_pool;
};
