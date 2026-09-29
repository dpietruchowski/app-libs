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

TEST(KeyboardBufferTest, NothingIsComposingUntilComposeIsCalled)
{
    KeyboardBuffer buffer;
    QSignalSpy compositionSpy(&buffer, &KeyboardBuffer::compositionChanged);

    buffer.typeKey("a");
    buffer.backspace();
    buffer.submit();

    EXPECT_FALSE(buffer.composing());
    EXPECT_EQ(buffer.compositionLength(), 0);
    EXPECT_EQ(compositionSpy.count(), 0);
}

TEST(KeyboardBufferTest, ComposeInsertsFirstCharacterAndMarksIt)
{
    KeyboardBuffer buffer;
    buffer.setText("ko");

    buffer.compose({ "t", "u", "v" });

    EXPECT_EQ(buffer.text(), "kot");
    EXPECT_TRUE(buffer.composing());
    EXPECT_EQ(buffer.compositionStart(), 2);
    EXPECT_EQ(buffer.compositionLength(), 1);
}

TEST(KeyboardBufferTest, ComposingSameGroupCyclesTheCharacter)
{
    KeyboardBuffer buffer;
    const QStringList group { "a", "b", "c" };

    buffer.compose(group);
    buffer.compose(group);
    buffer.compose(group);
    EXPECT_EQ(buffer.text(), "c");

    buffer.compose(group);
    EXPECT_EQ(buffer.text(), "a");
    EXPECT_EQ(buffer.cursorPosition(), 1);
}

TEST(KeyboardBufferTest, ComposingAnotherGroupCommitsThePreviousCharacter)
{
    KeyboardBuffer buffer;

    buffer.compose({ "a", "b", "c" });
    buffer.compose({ "a", "b", "c" });
    buffer.compose({ "d", "e", "f" });

    EXPECT_EQ(buffer.text(), "bd");
    EXPECT_EQ(buffer.compositionStart(), 1);
}

TEST(KeyboardBufferTest, SameGroupAfterCommitStartsNewCharacter)
{
    KeyboardBuffer buffer;
    const QStringList group { "a", "b", "c" };

    buffer.compose(group);
    buffer.commitComposition();
    buffer.compose(group);

    EXPECT_EQ(buffer.text(), "aa");
    EXPECT_TRUE(buffer.composing());
}

TEST(KeyboardBufferTest, SingleCharacterGroupTypesWithoutComposing)
{
    KeyboardBuffer buffer;

    buffer.compose({ "1" });
    buffer.compose({ "1" });

    EXPECT_EQ(buffer.text(), "11");
    EXPECT_FALSE(buffer.composing());
}

TEST(KeyboardBufferTest, ShiftOnceUppercasesWholeComposedCharacter)
{
    KeyboardBuffer buffer;
    const QStringList group { "a", "b", "c", "ą" };
    buffer.toggleShift();

    buffer.compose(group);
    buffer.compose(group);
    buffer.compose(group);
    buffer.compose(group);

    EXPECT_EQ(buffer.text(), "Ą");
    EXPECT_EQ(buffer.shiftState(), KeyboardBuffer::ShiftState::Off);

    buffer.compose({ "d", "e", "f" });
    EXPECT_EQ(buffer.text(), "Ąd");
}

TEST(KeyboardBufferTest, ComposeReplacesOnlyTheComposedCharacterInsideText)
{
    KeyboardBuffer buffer;
    buffer.setText("kt");
    buffer.setCursorPosition(1);
    const QStringList group { "m", "n", "o" };

    buffer.compose(group);
    buffer.compose(group);
    buffer.compose(group);

    EXPECT_EQ(buffer.text(), "kot");
    EXPECT_EQ(buffer.cursorPosition(), 2);
}

TEST(KeyboardBufferTest, EditingCommitsComposition)
{
    KeyboardBuffer buffer;
    const QStringList group { "a", "b", "c" };

    buffer.compose(group);
    buffer.insert(" ");
    EXPECT_FALSE(buffer.composing());

    buffer.compose(group);
    buffer.backspace();
    EXPECT_EQ(buffer.text(), "a ");
    EXPECT_FALSE(buffer.composing());

    buffer.compose(group);
    buffer.moveCursor(-1);
    EXPECT_FALSE(buffer.composing());

    buffer.compose(group);
    buffer.toggleShift();
    EXPECT_FALSE(buffer.composing());
}

TEST(KeyboardBufferTest, SubmitCommitsCompositionAndEmitsText)
{
    KeyboardBuffer buffer;
    QSignalSpy submittedSpy(&buffer, &KeyboardBuffer::submitted);

    buffer.compose({ "a", "b", "c" });
    buffer.submit();

    EXPECT_FALSE(buffer.composing());
    ASSERT_EQ(submittedSpy.count(), 1);
    EXPECT_EQ(submittedSpy.at(0).at(0).toString(), "a");
}

TEST(KeyboardBufferTest, SetTextEndsComposition)
{
    KeyboardBuffer buffer;

    buffer.compose({ "a", "b", "c" });
    buffer.setText("");
    buffer.compose({ "a", "b", "c" });

    EXPECT_EQ(buffer.text(), "a");
}

TEST(KeyboardBufferTest, ComposeWordInsertsTheWordAndMarksIt)
{
    KeyboardBuffer buffer;
    buffer.setText("a ");
    QSignalSpy compositionSpy(&buffer, &KeyboardBuffer::compositionChanged);

    buffer.composeWord("go");

    EXPECT_EQ(buffer.text(), "a go");
    EXPECT_EQ(buffer.cursorPosition(), 4);
    EXPECT_TRUE(buffer.composing());
    EXPECT_EQ(buffer.compositionStart(), 2);
    EXPECT_EQ(buffer.compositionLength(), 2);
    EXPECT_EQ(compositionSpy.count(), 1);
}

TEST(KeyboardBufferTest, ComposeWordReplacesTheComposedWord)
{
    KeyboardBuffer buffer;
    buffer.setText("xy");
    buffer.setCursorPosition(1);

    buffer.composeWord("go");
    buffer.composeWord("inn");
    buffer.composeWord("good");

    EXPECT_EQ(buffer.text(), "xgoody");
    EXPECT_EQ(buffer.cursorPosition(), 5);
    EXPECT_EQ(buffer.compositionLength(), 4);
}

TEST(KeyboardBufferTest, ComposeWordWithEmptyWordRemovesItAndStopsComposing)
{
    KeyboardBuffer buffer;
    buffer.setText("a ");

    buffer.composeWord("go");
    buffer.composeWord("");

    EXPECT_EQ(buffer.text(), "a ");
    EXPECT_EQ(buffer.cursorPosition(), 2);
    EXPECT_FALSE(buffer.composing());
}

TEST(KeyboardBufferTest, ComposeWordWithEmptyWordWhileIdleChangesNothing)
{
    KeyboardBuffer buffer;
    buffer.setText("a");
    QSignalSpy compositionSpy(&buffer, &KeyboardBuffer::compositionChanged);

    buffer.composeWord("");

    EXPECT_EQ(buffer.text(), "a");
    EXPECT_EQ(compositionSpy.count(), 0);
}

TEST(KeyboardBufferTest, CommitCompositionKeepsTheComposedWord)
{
    KeyboardBuffer buffer;

    buffer.composeWord("good");
    buffer.commitComposition();
    buffer.composeWord("go");

    EXPECT_EQ(buffer.text(), "goodgo");
    EXPECT_EQ(buffer.compositionStart(), 4);
}

TEST(KeyboardBufferTest, ShiftOnceCapitalizesOnlyTheFirstLetterOfTheWord)
{
    KeyboardBuffer buffer;
    buffer.toggleShift();

    buffer.composeWord("go");
    buffer.composeWord("good");

    EXPECT_EQ(buffer.text(), "Good");
    EXPECT_EQ(buffer.shiftState(), KeyboardBuffer::ShiftState::Off);
}

TEST(KeyboardBufferTest, LockedShiftUppercasesTheWholeWord)
{
    KeyboardBuffer buffer;
    buffer.lockShift();

    buffer.composeWord("good");

    EXPECT_EQ(buffer.text(), "GOOD");
    EXPECT_EQ(buffer.shiftState(), KeyboardBuffer::ShiftState::Locked);
}

TEST(KeyboardBufferTest, ComposeWordCommitsAPendingCharacter)
{
    KeyboardBuffer buffer;
    buffer.compose({ "a", "b", "c" });

    buffer.composeWord("go");

    EXPECT_EQ(buffer.text(), "ago");
    EXPECT_EQ(buffer.compositionStart(), 1);
}

TEST(KeyboardBufferTest, ComposeAfterComposeWordCommitsTheWord)
{
    KeyboardBuffer buffer;
    buffer.composeWord("go");

    buffer.compose({ ".", "," });

    EXPECT_EQ(buffer.text(), "go.");
    EXPECT_EQ(buffer.compositionStart(), 2);
    EXPECT_EQ(buffer.compositionLength(), 1);
}

TEST(KeyboardBufferTest, EditingCommitsComposedWord)
{
    KeyboardBuffer buffer;

    buffer.composeWord("go");
    buffer.insert(" ");
    EXPECT_FALSE(buffer.composing());
    EXPECT_EQ(buffer.text(), "go ");

    buffer.composeWord("in");
    buffer.backspace();
    EXPECT_FALSE(buffer.composing());
    EXPECT_EQ(buffer.text(), "go i");
}
