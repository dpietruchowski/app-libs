#include <QFont>
#include <gtest/gtest.h>

#include "diagram/textmeasurer.h"

namespace
{
QFont pixelFont(int pixelSize)
{
    QFont font;
    font.setPixelSize(pixelSize);
    return font;
}
}

TEST(FontTextMeasurerTest, EmptyTextHasNoWidth)
{
    const diagram::FontTextMeasurer measurer(pixelFont(14));

    EXPECT_DOUBLE_EQ(measurer.width(QString()), 0);
}

TEST(FontTextMeasurerTest, LongerTextIsWider)
{
    const diagram::FontTextMeasurer measurer(pixelFont(14));

    EXPECT_GT(measurer.width(QStringLiteral("Auth")), 0);
    EXPECT_GT(measurer.width(QStringLiteral("Authorization")),
              measurer.width(QStringLiteral("Auth")));
}

TEST(FontTextMeasurerTest, ASpaceTakesWidth)
{
    const diagram::FontTextMeasurer measurer(pixelFont(14));

    EXPECT_GT(measurer.width(QStringLiteral("a b")), measurer.width(QStringLiteral("ab")));
}

TEST(FontTextMeasurerTest, LargerFontIsWiderAndTaller)
{
    const diagram::FontTextMeasurer small(pixelFont(12));
    const diagram::FontTextMeasurer large(pixelFont(24));

    EXPECT_GT(large.width(QStringLiteral("Client")), small.width(QStringLiteral("Client")));
    EXPECT_GT(large.lineHeight(), small.lineHeight());
    EXPECT_GT(small.lineHeight(), 0);
}
