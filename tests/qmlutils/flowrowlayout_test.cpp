#include <QFont>
#include <gtest/gtest.h>

#include "qmlutils/flowrowlayout.h"

namespace
{
FlowRowLayout makePackingLayout(const QList<qreal>& widths, qreal containerWidth,
                                qreal spacing = 10, qreal rowHeight = 52)
{
    FlowRowLayout layout;
    layout.setSpacing(spacing);
    layout.setRowHeight(rowHeight);
    layout.setContainerWidth(containerWidth);
    layout.setItemWidths(widths);
    return layout;
}
}

TEST(FlowRowLayoutPacking, EmptyLayoutHasNoRows)
{
    FlowRowLayout layout = makePackingLayout({}, 300);

    EXPECT_EQ(layout.rowCount(), 0);
    EXPECT_EQ(layout.itemCount(), 0);
    EXPECT_DOUBLE_EQ(layout.contentHeight(), 0);
    EXPECT_EQ(layout.rowOfItem(0), -1);
}

TEST(FlowRowLayoutPacking, ItemsThatFitStayOnOneRow)
{
    FlowRowLayout layout = makePackingLayout({ 100, 100 }, 250);

    ASSERT_EQ(layout.rowCount(), 1);
    EXPECT_EQ(layout.row(0).firstIndex, 0);
    EXPECT_EQ(layout.row(0).count, 2);
    EXPECT_DOUBLE_EQ(layout.row(0).width, 210);
}

TEST(FlowRowLayoutPacking, WrapsWhenNextItemExceedsContainer)
{
    FlowRowLayout layout = makePackingLayout({ 100, 100, 100 }, 250);

    ASSERT_EQ(layout.rowCount(), 2);
    EXPECT_EQ(layout.row(0).firstIndex, 0);
    EXPECT_EQ(layout.row(0).count, 2);
    EXPECT_DOUBLE_EQ(layout.row(0).width, 210);
    EXPECT_EQ(layout.row(1).firstIndex, 2);
    EXPECT_EQ(layout.row(1).count, 1);
    EXPECT_DOUBLE_EQ(layout.row(1).width, 100);
}

TEST(FlowRowLayoutPacking, RowsAreStackedByRowHeightAndSpacing)
{
    FlowRowLayout layout = makePackingLayout({ 100, 100, 100 }, 250);

    EXPECT_DOUBLE_EQ(layout.row(0).y, 0);
    EXPECT_DOUBLE_EQ(layout.row(1).y, 62);
    EXPECT_DOUBLE_EQ(layout.contentHeight(), 114);
}

TEST(FlowRowLayoutPacking, ItemXRestartsOnEveryRow)
{
    FlowRowLayout layout = makePackingLayout({ 100, 100, 100 }, 250);

    EXPECT_DOUBLE_EQ(layout.itemX(0), 0);
    EXPECT_DOUBLE_EQ(layout.itemX(1), 110);
    EXPECT_DOUBLE_EQ(layout.itemX(2), 0);
}

TEST(FlowRowLayoutPacking, RowOfItemMapsEveryIndex)
{
    FlowRowLayout layout = makePackingLayout({ 100, 100, 100 }, 250);

    EXPECT_EQ(layout.rowOfItem(0), 0);
    EXPECT_EQ(layout.rowOfItem(1), 0);
    EXPECT_EQ(layout.rowOfItem(2), 1);
    EXPECT_EQ(layout.rowOfItem(3), -1);
}

TEST(FlowRowLayoutPacking, OversizedItemIsClampedToContainer)
{
    FlowRowLayout layout = makePackingLayout({ 300 }, 200);

    ASSERT_EQ(layout.rowCount(), 1);
    EXPECT_DOUBLE_EQ(layout.itemWidth(0), 200);
    EXPECT_DOUBLE_EQ(layout.itemNaturalWidth(0), 300);
}

TEST(FlowRowLayoutPacking, OversizedItemGetsItsOwnRow)
{
    FlowRowLayout layout = makePackingLayout({ 50, 300 }, 200, 0);

    ASSERT_EQ(layout.rowCount(), 2);
    EXPECT_EQ(layout.row(0).count, 1);
    EXPECT_EQ(layout.row(1).firstIndex, 1);
    EXPECT_DOUBLE_EQ(layout.itemWidth(1), 200);
}

TEST(FlowRowLayoutPacking, WithoutContainerWidthNothingWraps)
{
    FlowRowLayout layout = makePackingLayout({ 100, 100, 100 }, 0);

    ASSERT_EQ(layout.rowCount(), 1);
    EXPECT_EQ(layout.row(0).count, 3);
    EXPECT_DOUBLE_EQ(layout.itemWidth(0), 100);
}

TEST(FlowRowLayoutPacking, ResizingRepacksWithoutChangingNaturalWidths)
{
    FlowRowLayout layout = makePackingLayout({ 100, 100, 100 }, 250);
    ASSERT_EQ(layout.rowCount(), 2);

    layout.setContainerWidth(400);

    EXPECT_EQ(layout.rowCount(), 1);
    EXPECT_DOUBLE_EQ(layout.itemNaturalWidth(0), 100);

    layout.setContainerWidth(150);

    EXPECT_EQ(layout.rowCount(), 3);
    EXPECT_DOUBLE_EQ(layout.itemNaturalWidth(0), 100);
}

TEST(FlowRowLayoutMeasuring, WidthComesFromTheWidestLine)
{
    FlowRowLayout layout;
    layout.setContainerWidth(10000);
    layout.setItems({ { { "a", "a very long second line" }, 0 }, { { "a", "a" }, 0 } });

    EXPECT_GT(layout.itemNaturalWidth(0), layout.itemNaturalWidth(1));
}

TEST(FlowRowLayoutMeasuring, LongerTextMeasuresWider)
{
    FlowRowLayout layout;
    layout.setContainerWidth(10000);
    layout.setItems({ { { "kot" }, 0 }, { { "kotoprzeprowadzka" }, 0 } });

    EXPECT_GT(layout.itemNaturalWidth(1), layout.itemNaturalWidth(0));
}

TEST(FlowRowLayoutMeasuring, ExtraWidthIsAddedToEveryItem)
{
    FlowRowLayout bare;
    bare.setContainerWidth(10000);
    bare.setItems({ { { "kot" }, 0 } });

    FlowRowLayout padded;
    padded.setContainerWidth(10000);
    padded.setItems({ { { "kot" }, 40 } });

    EXPECT_DOUBLE_EQ(padded.itemNaturalWidth(0), bare.itemNaturalWidth(0) + 40);
}

TEST(FlowRowLayoutMeasuring, EachLineUsesItsOwnFont)
{
    QFont small;
    small.setPixelSize(10);
    QFont large;
    large.setPixelSize(40);

    FlowRowLayout smallFirst;
    smallFirst.setContainerWidth(10000);
    smallFirst.setLineFonts({ small, large });
    smallFirst.setItems({ { { "kotoprzeprowadzka", "a" }, 0 } });

    FlowRowLayout largeFirst;
    largeFirst.setContainerWidth(10000);
    largeFirst.setLineFonts({ large, small });
    largeFirst.setItems({ { { "kotoprzeprowadzka", "a" }, 0 } });

    EXPECT_GT(largeFirst.itemNaturalWidth(0), smallFirst.itemNaturalWidth(0));
}

TEST(FlowRowLayoutMeasuring, LinesBeyondTheFontListReuseTheLastFont)
{
    QFont large;
    large.setPixelSize(40);

    FlowRowLayout layout;
    layout.setContainerWidth(10000);
    layout.setLineFonts({ large });
    layout.setItems({ { { "a", "kotoprzeprowadzka" }, 0 }, { { "a", "a" }, 0 } });

    EXPECT_GT(layout.itemNaturalWidth(0), layout.itemNaturalWidth(1));
}

TEST(FlowRowLayoutMeasuring, MeasuredItemsWrapIntoRows)
{
    QFont font;
    font.setPixelSize(20);

    FlowRowLayout layout;
    layout.setSpacing(8);
    layout.setRowHeight(52);
    layout.setLineFonts({ font });
    layout.setContainerWidth(200);
    layout.setItems({ { { "kotoprzeprowadzka" }, 20 },
                      { { "kotoprzeprowadzka" }, 20 },
                      { { "kotoprzeprowadzka" }, 20 } });

    EXPECT_GT(layout.rowCount(), 1);
    EXPECT_EQ(layout.itemCount(), 3);
    for (int i = 0; i < layout.itemCount(); ++i)
    {
        EXPECT_LE(layout.itemX(i) + layout.itemWidth(i), layout.containerWidth());
    }
}
