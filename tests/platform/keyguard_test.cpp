#include "platform/keyguard.h"

#include <gtest/gtest.h>

namespace
{

struct KeyguardRecorder
{
    explicit KeyguardRecorder(Keyguard& keyguard)
    {
        QObject::connect(&keyguard, &Keyguard::stateChanged, [this] { ++stateChanges; });
        QObject::connect(&keyguard, &Keyguard::unlockFinished,
                         [this](bool succeeded) { unlockResults << succeeded; });
    }

    int stateChanges = 0;
    QList<bool> unlockResults;
};

}  // namespace

TEST(KeyguardTest, WithoutBackend_DeviceIsNotLockedAndNothingIsShownOverLock)
{
    Keyguard keyguard;

    EXPECT_FALSE(keyguard.locked());
    EXPECT_FALSE(keyguard.shownOverLock());
}

TEST(KeyguardTest, UpdateWithNewState_ExposesItAndAnnouncesOnce)
{
    Keyguard keyguard;
    KeyguardRecorder recorder(keyguard);

    keyguard.update(true, true);

    EXPECT_TRUE(keyguard.locked());
    EXPECT_TRUE(keyguard.shownOverLock());
    EXPECT_EQ(recorder.stateChanges, 1);
}

TEST(KeyguardTest, UpdateWithSameState_IsNotAnnounced)
{
    Keyguard keyguard;
    keyguard.update(true, false);
    KeyguardRecorder recorder(keyguard);

    keyguard.update(true, false);

    EXPECT_EQ(recorder.stateChanges, 0);
}

TEST(KeyguardTest, UpdateWithOnlyOneFlagChanged_IsAnnounced)
{
    Keyguard keyguard;
    keyguard.update(true, true);
    KeyguardRecorder recorder(keyguard);

    keyguard.update(true, false);

    EXPECT_TRUE(keyguard.locked());
    EXPECT_FALSE(keyguard.shownOverLock());
    EXPECT_EQ(recorder.stateChanges, 1);
}

TEST(KeyguardTest, WithoutBackend_RequestUnlockSucceedsBecauseNothingIsLocked)
{
    Keyguard keyguard;
    KeyguardRecorder recorder(keyguard);

    keyguard.requestUnlock();

    EXPECT_EQ(recorder.unlockResults, QList<bool> { true });
}

TEST(KeyguardTest, FinishedUnlock_ReportsItsOutcomeEachTime)
{
    Keyguard keyguard;
    KeyguardRecorder recorder(keyguard);

    keyguard.finishUnlock(false);
    keyguard.finishUnlock(true);

    EXPECT_EQ(recorder.unlockResults, (QList<bool> { false, true }));
    EXPECT_EQ(recorder.stateChanges, 0);
}
