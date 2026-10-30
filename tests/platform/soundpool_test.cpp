#include "platform/soundpool.h"

#include <gtest/gtest.h>

TEST(SoundPoolTest, WithoutBackend_IsNotAvailable)
{
    SoundPool pool;

    EXPECT_FALSE(pool.isAvailable());
}

TEST(SoundPoolTest, WithoutBackend_LoadsNothing)
{
    SoundPool pool;

    EXPECT_EQ(pool.load(QStringLiteral(":/sounds/missing.wav")), 0);
}

TEST(SoundPoolTest, WithoutBackend_PlayingIsHarmless)
{
    SoundPool pool;

    EXPECT_FALSE(pool.play(1));
    EXPECT_FALSE(pool.play(0, 0.5f, 2.0f));
}
