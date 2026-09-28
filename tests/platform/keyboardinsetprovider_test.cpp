#include "platform/keyboardinsetprovider.h"

#include <gtest/gtest.h>

TEST(KeyboardInsetProviderTest, Fresh_RemembersNoHeight)
{
    KeyboardInsetProvider provider;

    EXPECT_EQ(provider.bottom(), 0);
    EXPECT_EQ(provider.rememberedHeight(), 0);
}

TEST(KeyboardInsetProviderTest, SeededHeight_IsAnnouncedOnlyWhenItChanges)
{
    KeyboardInsetProvider provider;
    int announced = 0;
    QObject::connect(&provider, &KeyboardInsetProvider::rememberedHeightChanged,
                     [&announced] { ++announced; });

    provider.setRememberedHeight(310);
    provider.setRememberedHeight(310);

    EXPECT_EQ(provider.rememberedHeight(), 310);
    EXPECT_EQ(announced, 1);
}
