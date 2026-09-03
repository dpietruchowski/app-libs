#include <gtest/gtest.h>

#include <QString>

TEST(ExampleTest, QStringConcatenation)
{
    const QString greeting = QStringLiteral("__APP_NAME__");

    EXPECT_EQ(greeting + QStringLiteral(" runs"), QStringLiteral("__APP_NAME__ runs"));
}
