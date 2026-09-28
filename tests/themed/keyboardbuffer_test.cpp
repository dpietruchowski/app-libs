#include <QSignalSpy>
#include <gtest/gtest.h>

#include "keyboardbuffer.h"

TEST(KeyboardBufferTest, InsertPutsTextAtCursorAndMovesCursorPastIt)
{
    KeyboardBuffer buffer;
    buffer.setText("kot");
    buffer.setCursorPosition(1);

    buffer.insert("ró");

    EXPECT_EQ(buffer.text(), "króot");
    EXPECT_EQ(buffer.cursorPosition(), 3);
}

TEST(KeyboardBufferTest, SetTextPlacesCursorAtEnd)
{
    KeyboardBuffer buffer;

    buffer.setText("dom");

    EXPECT_EQ(buffer.cursorPosition(), 3);
}

TEST(KeyboardBufferTest, CursorPositionIsClampedToText)
{
    KeyboardBuffer buffer;
    buffer.setText("dom");

    buffer.setCursorPosition(10);
    EXPECT_EQ(buffer.cursorPosition(), 3);

    buffer.setCursorPosition(-4);
    EXPECT_EQ(buffer.cursorPosition(), 0);
}

TEST(KeyboardBufferTest, BackspaceRemovesCharacterBeforeCursor)
{
    KeyboardBuffer buffer;
    buffer.setText("kota");
    buffer.setCursorPosition(2);

    buffer.backspace();

    EXPECT_EQ(buffer.text(), "kta");
    EXPECT_EQ(buffer.cursorPosition(), 1);
}

TEST(KeyboardBufferTest, BackspaceAtStartChangesNothing)
{
    KeyboardBuffer buffer;
    buffer.setText("kot");
    buffer.setCursorPosition(0);
    QSignalSpy textSpy(&buffer, &KeyboardBuffer::textChanged);

    buffer.backspace();

    EXPECT_EQ(buffer.text(), "kot");
    EXPECT_EQ(textSpy.count(), 0);
}

TEST(KeyboardBufferTest, BackspaceRemovesWholeSurrogatePair)
{
    KeyboardBuffer buffer;
    buffer.setText(QString("a") + QString::fromUcs4(U"\U0001F600"));

    buffer.backspace();

    EXPECT_EQ(buffer.text(), "a");
    EXPECT_EQ(buffer.cursorPosition(), 1);
}

TEST(KeyboardBufferTest, DeleteForwardRemovesCharacterAfterCursor)
{
    KeyboardBuffer buffer;
    buffer.setText("kota");
    buffer.setCursorPosition(1);

    buffer.deleteForward();

    EXPECT_EQ(buffer.text(), "kta");
    EXPECT_EQ(buffer.cursorPosition(), 1);
}

TEST(KeyboardBufferTest, DeleteForwardAtEndChangesNothing)
{
    KeyboardBuffer buffer;
    buffer.setText("kot");

    buffer.deleteForward();

    EXPECT_EQ(buffer.text(), "kot");
}

TEST(KeyboardBufferTest, MoveCursorStepsOverSurrogatePairsAndStopsAtEdges)
{
    KeyboardBuffer buffer;
    buffer.setText(QString("a") + QString::fromUcs4(U"\U0001F600") + "b");

    buffer.moveCursor(-2);
    EXPECT_EQ(buffer.cursorPosition(), 1);

    buffer.moveCursor(-5);
    EXPECT_EQ(buffer.cursorPosition(), 0);

    buffer.moveCursor(2);
    EXPECT_EQ(buffer.cursorPosition(), 3);
}

TEST(KeyboardBufferTest, TypeKeyWithoutShiftKeepsCase)
{
    KeyboardBuffer buffer;

    buffer.typeKey("ą");

    EXPECT_EQ(buffer.text(), "ą");
}

TEST(KeyboardBufferTest, ShiftOnceUppercasesOnlyNextKey)
{
    KeyboardBuffer buffer;
    buffer.toggleShift();

    buffer.typeKey("ż");
    buffer.typeKey("a");

    EXPECT_EQ(buffer.text(), "Ża");
    EXPECT_EQ(buffer.shiftState(), KeyboardBuffer::ShiftState::Off);
}

TEST(KeyboardBufferTest, LockedShiftUppercasesUntilToggled)
{
    KeyboardBuffer buffer;
    buffer.lockShift();

    buffer.typeKey("a");
    buffer.typeKey("b");
    buffer.toggleShift();
    buffer.typeKey("c");

    EXPECT_EQ(buffer.text(), "ABc");
}

TEST(KeyboardBufferTest, ToggleShiftCyclesBetweenOffAndOnce)
{
    KeyboardBuffer buffer;
    QSignalSpy shiftSpy(&buffer, &KeyboardBuffer::shiftStateChanged);

    buffer.toggleShift();
    EXPECT_EQ(buffer.shiftState(), KeyboardBuffer::ShiftState::Once);
    EXPECT_TRUE(buffer.upperCase());

    buffer.toggleShift();
    EXPECT_EQ(buffer.shiftState(), KeyboardBuffer::ShiftState::Off);
    EXPECT_FALSE(buffer.upperCase());

    EXPECT_EQ(shiftSpy.count(), 2);
}

TEST(KeyboardBufferTest, InsertIgnoresShift)
{
    KeyboardBuffer buffer;
    buffer.toggleShift();

    buffer.insert("a");

    EXPECT_EQ(buffer.text(), "a");
    EXPECT_EQ(buffer.shiftState(), KeyboardBuffer::ShiftState::Once);
}

TEST(KeyboardBufferTest, SubmitEmitsTrimmedText)
{
    KeyboardBuffer buffer;
    buffer.setText("  kot ");
    QSignalSpy submittedSpy(&buffer, &KeyboardBuffer::submitted);

    buffer.submit();

    ASSERT_EQ(submittedSpy.count(), 1);
    EXPECT_EQ(submittedSpy.at(0).at(0).toString(), "kot");
    EXPECT_EQ(buffer.text(), "  kot ");
}

TEST(KeyboardBufferTest, SubmitOfBlankTextEmitsNothing)
{
    KeyboardBuffer buffer;
    buffer.setText("   ");
    QSignalSpy submittedSpy(&buffer, &KeyboardBuffer::submitted);

    buffer.submit();

    EXPECT_EQ(submittedSpy.count(), 0);
}

TEST(KeyboardBufferTest, ClearEmptiesTextAndResetsCursor)
{
    KeyboardBuffer buffer;
    buffer.setText("kot");

    buffer.clear();

    EXPECT_TRUE(buffer.text().isEmpty());
    EXPECT_EQ(buffer.cursorPosition(), 0);
}
