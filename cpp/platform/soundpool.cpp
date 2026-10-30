#include "soundpool.h"

#include <QtGlobal>

#if defined(Q_OS_ANDROID)

#include "android/soundpoolandroid.h"

struct SoundPool::Impl
{
    explicit Impl(int maxStreams)
        : backend(maxStreams)
    {
    }

    SoundPoolAndroid backend;
};

SoundPool::SoundPool(int maxStreams)
    : m_impl(std::make_unique<Impl>(maxStreams))
{
}

SoundPool::~SoundPool() = default;

bool SoundPool::isAvailable() const { return m_impl->backend.isAvailable(); }

int SoundPool::load(const QString& path) { return m_impl->backend.load(path); }

bool SoundPool::play(int soundId, float volume, float rate)
{
    return m_impl->backend.play(soundId, volume, rate);
}

#else

struct SoundPool::Impl
{
};

SoundPool::SoundPool(int maxStreams)
    : m_impl(std::make_unique<Impl>())
{
    Q_UNUSED(maxStreams);
}

SoundPool::~SoundPool() = default;

bool SoundPool::isAvailable() const { return false; }

int SoundPool::load(const QString& path)
{
    Q_UNUSED(path);
    return 0;
}

bool SoundPool::play(int soundId, float volume, float rate)
{
    Q_UNUSED(soundId);
    Q_UNUSED(volume);
    Q_UNUSED(rate);
    return false;
}

#endif
