#pragma once

#include <QString>

#include <memory>

// Low-latency playback of short sound effects, several at once.
//
// Android is backed by the system SoundPool. A sound is loaded once and played
// by the id load() returns; a path inside the Qt resources (":/...") is copied
// to the cache first because SoundPool only reads files. Platforms without a
// backend load nothing and play nothing, so callers need no checks.
class SoundPool final
{
public:
    explicit SoundPool(int maxStreams = 8);
    ~SoundPool();

    SoundPool(const SoundPool&) = delete;
    SoundPool& operator=(const SoundPool&) = delete;

    bool isAvailable() const;

    int load(const QString& path);
    bool play(int soundId, float volume = 1.0f, float rate = 1.0f);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
