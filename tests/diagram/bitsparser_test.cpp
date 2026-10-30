#include <gtest/gtest.h>

#include "diagram/bitsparser.h"

using namespace diagram;

namespace
{
const QString kPacket = QStringLiteral("@startbits\n"
                                       "title struct Packet (12 B)\n"
                                       "bytes 4\n"
                                       "field  0..0   char tag\n"
                                       "pad    1..3\n"
                                       "field  4..7   int  id\n"
                                       "field  8..9   short len\n"
                                       "pad    10..11\n"
                                       "@endbits\n");

const QString kEntry = QStringLiteral("@startbits\n"
                                      "title PTE (x86-64)\n"
                                      "bits 64\n"
                                      "field 0      P   \"present\"\n"
                                      "field 1      RW\n"
                                      "field 12..51 PFN \"frame number\"\n"
                                      "field 63     NX\n"
                                      "@endbits\n");

BitsDiagram parsed(const QString& text)
{
    const auto result = BitsParser::parse(text);
    EXPECT_TRUE(result.isSuccess()) << (result.isFailure() ? result.error().toStdString() : "");
    return result.isSuccess() ? result.value() : BitsDiagram {};
}

QString parseError(const QString& text)
{
    const auto result = BitsParser::parse(text);
    EXPECT_TRUE(result.isFailure());
    return result.isFailure() ? result.error() : QString();
}
}

TEST(BitsParserTest, ReadsAMemoryLayoutWithFieldsAndPadding)
{
    const auto diagram = parsed(kPacket);

    EXPECT_EQ(diagram.title, QStringLiteral("struct Packet (12 B)"));
    EXPECT_EQ(diagram.mode, BitsMode::Bytes);
    EXPECT_EQ(diagram.width, 4);
    ASSERT_EQ(diagram.spans.size(), 5);
    EXPECT_EQ(diagram.spans.at(0).kind, BitsSpanKind::Field);
    EXPECT_EQ(diagram.spans.at(0).low, 0);
    EXPECT_EQ(diagram.spans.at(0).high, 0);
    EXPECT_EQ(diagram.spans.at(0).label, QStringLiteral("char tag"));
    EXPECT_EQ(diagram.spans.at(0).line, 4);
    EXPECT_EQ(diagram.spans.at(1).kind, BitsSpanKind::Padding);
    EXPECT_EQ(diagram.spans.at(1).low, 1);
    EXPECT_EQ(diagram.spans.at(1).high, 3);
    EXPECT_TRUE(diagram.spans.at(1).label.isEmpty());
    EXPECT_EQ(diagram.spans.at(3).label, QStringLiteral("short len"));
    EXPECT_EQ(diagram.spans.at(4).high, 11);
    EXPECT_TRUE(diagram.warnings.isEmpty());
}

TEST(BitsParserTest, ReadsARegisterWithDescriptionsAndAReversedRange)
{
    const auto diagram = parsed(kEntry);

    EXPECT_EQ(diagram.mode, BitsMode::Bits);
    EXPECT_EQ(diagram.width, 64);
    ASSERT_EQ(diagram.spans.size(), 4);
    EXPECT_EQ(diagram.spans.at(0).label, QStringLiteral("P"));
    EXPECT_EQ(diagram.spans.at(0).description, QStringLiteral("present"));
    EXPECT_TRUE(diagram.spans.at(1).description.isEmpty());
    EXPECT_EQ(diagram.spans.at(2).low, 12);
    EXPECT_EQ(diagram.spans.at(2).high, 51);
    EXPECT_EQ(diagram.spans.at(2).description, QStringLiteral("frame number"));
}

TEST(BitsParserTest, ARangeMayBeWrittenInEitherOrder)
{
    const auto diagram = parsed(QStringLiteral("bits 64\nfield 51..12 PFN\npad 63..60\n"));

    EXPECT_EQ(diagram.spans.at(0).low, 12);
    EXPECT_EQ(diagram.spans.at(0).high, 51);
    EXPECT_EQ(diagram.spans.at(1).low, 60);
    EXPECT_EQ(diagram.spans.at(1).high, 63);
}

TEST(BitsParserTest, ALabelRunsToTheDescriptionAndKeepsItsSpaces)
{
    const auto diagram
        = parsed(QStringLiteral("bytes 8\nfield 0..7 unsigned long  value \"a b\"\n"));

    EXPECT_EQ(diagram.spans.at(0).label, QStringLiteral("unsigned long  value"));
    EXPECT_EQ(diagram.spans.at(0).description, QStringLiteral("a b"));
}

TEST(BitsParserTest, CommentsBlankLinesAndEitherMarkerPairAreSkipped)
{
    const auto diagram = parsed(QStringLiteral("@startuml\n"
                                               "' comment\n"
                                               "/' block\ncomment '/\n"
                                               "\n"
                                               "bits 8\n"
                                               "field 0..7 byte\n"
                                               "@enduml\n"));

    EXPECT_EQ(diagram.spans.size(), 1);
    EXPECT_TRUE(diagram.warnings.isEmpty());
}

TEST(BitsParserTest, BackslashNBreaksALineInADescription)
{
    const auto diagram = parsed(QStringLiteral("bits 8\nfield 0..7 x \"one\\ntwo\"\n"));

    EXPECT_EQ(diagram.spans.at(0).description, QStringLiteral("one\ntwo"));
}

TEST(BitsParserTest, TheLimitsOfTheLayoutAreAccepted)
{
    EXPECT_EQ(parsed(QStringLiteral("bits 1\nfield 0 x\n")).width, 1);
    EXPECT_EQ(parsed(QStringLiteral("bits 128\nfield 127 x\n")).width, 128);
    EXPECT_EQ(parsed(QStringLiteral("bytes 16\nfield 1023 x\n")).width, 16);
}

TEST(BitsParserTest, AFieldBeforeTheLayoutLineIsAnError)
{
    EXPECT_EQ(parseError(QStringLiteral("title T\nfield 0..7 x\nbits 8\n")),
              QStringLiteral("Line 2: the layout is missing. Start with \"bits N\" or \"bytes W\" "
                             "before the first field or pad."));
    EXPECT_TRUE(parseError(QStringLiteral("pad 0..1\n"))
                    .startsWith(QStringLiteral("Line 1: the layout is missing.")));
}

TEST(BitsParserTest, ATextWithoutALayoutLineOrFieldsIsAnError)
{
    EXPECT_EQ(parseError(QStringLiteral("title Only a title\n")),
              QStringLiteral("Line 1: the layout is missing. Start with \"bits N\" or \"bytes W\" "
                             "before the fields."));
    EXPECT_EQ(parseError(QStringLiteral("bits 8\n\n")),
              QStringLiteral("Line 1: the diagram has no fields. Add field lines after "
                             "\"bits 8\"."));
    EXPECT_EQ(parseError(QStringLiteral("\n \n")), QStringLiteral("The diagram is empty"));
}

TEST(BitsParserTest, ASecondLayoutLineIsAnErrorThatNamesTheFirst)
{
    EXPECT_EQ(parseError(QStringLiteral("bits 8\nfield 0 x\nbytes 4\n")),
              QStringLiteral("Line 3: the layout is already set to \"bits 8\" at line 1. Use one "
                             "\"bits N\" or \"bytes W\" line."));
    EXPECT_TRUE(parseError(QStringLiteral("bytes 4\nbytes 8\n"))
                    .contains(QStringLiteral("already set to \"bytes 4\" at line 1")));
}

TEST(BitsParserTest, AnOutOfRangeLayoutSizeIsAnError)
{
    EXPECT_EQ(parseError(QStringLiteral("bits 0\n")),
              QStringLiteral("Line 1: \"bits 0\" is out of range. Use bits from 1 to 128."));
    EXPECT_EQ(parseError(QStringLiteral("bits 129\n")),
              QStringLiteral("Line 1: \"bits 129\" is out of range. Use bits from 1 to 128."));
    EXPECT_EQ(parseError(QStringLiteral("title T\nbytes 17\n")),
              QStringLiteral("Line 2: \"bytes 17\" is out of range. Use bytes from 1 to 16."));
    EXPECT_TRUE(parseError(QStringLiteral("bytes 0\n")).contains(QStringLiteral("out of range")));
}

TEST(BitsParserTest, AnIndexOutsideTheRegisterIsAnError)
{
    EXPECT_EQ(parseError(QStringLiteral("bits 32\nfield 40 x\n")),
              QStringLiteral("Line 2: bit 40 is outside \"bits 32\". Use bits 0 to 31."));
    EXPECT_EQ(parseError(QStringLiteral("bits 32\nfield 28..32 x\n")),
              QStringLiteral("Line 2: bits 28..32 is outside \"bits 32\". Use bits 0 to 31."));
    EXPECT_EQ(parseError(QStringLiteral("bits 8\npad 9..2\n")),
              QStringLiteral("Line 2: bits 2..9 is outside \"bits 8\". Use bits 0 to 7."));
}

TEST(BitsParserTest, AnIndexBeyondTheLargestMemoryLayoutIsAnError)
{
    EXPECT_EQ(parseError(QStringLiteral("bytes 4\nfield 1024 x\n")),
              QStringLiteral("Line 2: byte 1024 is beyond the largest layout of 1024 bytes. Use "
                             "bytes 0 to 1023."));
}

TEST(BitsParserTest, OverlappingSpansAreAnErrorThatNamesTheOtherOne)
{
    EXPECT_EQ(parseError(QStringLiteral("bits 16\nfield 0..3 A\nfield 3..5 B\n")),
              QStringLiteral("Line 3: bits 3..5 overlaps the field \"A\" at line 2 (bits 0..3). "
                             "Give every bit to one field or pad."));
    EXPECT_EQ(parseError(QStringLiteral("bytes 4\npad 1..2\nfield 2 x\n")),
              QStringLiteral("Line 3: byte 2 overlaps the padding at line 2 (bytes 1..2). Give "
                             "every byte to one field or pad."));
    EXPECT_TRUE(parseError(QStringLiteral("bits 8\nfield 0..7 A\npad 7\n"))
                    .contains(QStringLiteral("overlaps the field \"A\"")));
}

TEST(BitsParserTest, AdjacentSpansDoNotOverlap)
{
    EXPECT_EQ(parsed(QStringLiteral("bits 8\nfield 0..3 A\nfield 4..7 B\n")).spans.size(), 2);
}

TEST(BitsParserTest, MalformedLinesNameTheFormTheyShouldHave)
{
    EXPECT_EQ(parseError(QStringLiteral("bits 8\nfield 0..7\n")),
              QStringLiteral("Line 2: \"field 0..7\" is not a valid \"field\" statement. Write it "
                             "as: field 0..7 label \"description\", with the description "
                             "optional."));
    EXPECT_TRUE(parseError(QStringLiteral("bits 8\nfield 0..7 \"only text\"\n"))
                    .contains(QStringLiteral("is not a valid \"field\" statement")));
    EXPECT_TRUE(parseError(QStringLiteral("bits 8\nfield a..b x\n"))
                    .contains(QStringLiteral("is not a valid \"field\" statement")));
    EXPECT_EQ(parseError(QStringLiteral("bits 8\npad\n")),
              QStringLiteral("Line 2: \"pad\" is not a valid \"pad\" statement. Write it as: pad "
                             "8..15."));
    EXPECT_EQ(parseError(QStringLiteral("bits many\n")),
              QStringLiteral("Line 1: \"bits many\" is not a valid \"bits\" statement. Write it "
                             "as: bits N, with N from 1 to 128."));
    EXPECT_TRUE(parseError(QStringLiteral("bytes\n"))
                    .contains(QStringLiteral("is not a valid \"bytes\" statement")));
    EXPECT_TRUE(parseError(QStringLiteral("bits 8\ntitle\n"))
                    .contains(QStringLiteral("is not a valid \"title\" statement")));
}

TEST(BitsParserTest, AnUnknownStatementListsWhatABitsDiagramUnderstands)
{
    const QString error = parseError(QStringLiteral("bits 8\nwhatever 1\n"));

    EXPECT_TRUE(error.startsWith(QStringLiteral("Line 2: unknown statement \"whatever 1\". A bits "
                                                "diagram here understands: ")))
        << error.toStdString();
}

TEST(BitsParserTest, AnUnclosedBlockCommentIsAnError)
{
    EXPECT_EQ(parseError(QStringLiteral("bits 8\n/' never closed\nfield 0 x\n")),
              QStringLiteral("Line 2: \"comment\" block is not closed"));
}

TEST(BitsParserTest, TooManySpansAreRejectedAtTheLineThatCrossesTheLimit)
{
    QString text = QStringLiteral("bytes 16\n");
    for (int index = 0; index < 301; ++index)
        text += QStringLiteral("pad %1\n").arg(index);

    EXPECT_EQ(parseError(text),
              QStringLiteral("Line 302: the diagram has more than 300 fields and pads"));
}

TEST(BitsParserTest, TextLongerThanTheLimitIsRejected)
{
    EXPECT_TRUE(parseError(QStringLiteral("bits 8\n") + QString(20001, QLatin1Char(' '))
                           + QStringLiteral("field 0 x\n"))
                    .contains(QStringLiteral("longer than 20000 characters")));
}
