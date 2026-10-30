#include <gtest/gtest.h>

#include "diagram/diagramparser.h"

using namespace diagram;

namespace
{
bool isActivity(const QString& text)
{
    const auto result = DiagramParser::parse(text);
    EXPECT_TRUE(result.isSuccess()) << (result.isFailure() ? result.error().toStdString() : "");
    return result.isSuccess() && std::holds_alternative<ActivityDiagram>(result.value());
}

bool isSequence(const QString& text)
{
    const auto result = DiagramParser::parse(text);
    EXPECT_TRUE(result.isSuccess()) << (result.isFailure() ? result.error().toStdString() : "");
    return result.isSuccess() && std::holds_alternative<SequenceDiagram>(result.value());
}

bool isBlock(const QString& text)
{
    const auto result = DiagramParser::parse(text);
    EXPECT_TRUE(result.isSuccess()) << (result.isFailure() ? result.error().toStdString() : "");
    return result.isSuccess() && std::holds_alternative<BlockDiagram>(result.value());
}

bool isState(const QString& text)
{
    const auto result = DiagramParser::parse(text);
    EXPECT_TRUE(result.isSuccess()) << (result.isFailure() ? result.error().toStdString() : "");
    return result.isSuccess() && std::holds_alternative<StateDiagram>(result.value());
}

bool isClass(const QString& text)
{
    const auto result = DiagramParser::parse(text);
    EXPECT_TRUE(result.isSuccess()) << (result.isFailure() ? result.error().toStdString() : "");
    return result.isSuccess() && std::holds_alternative<ClassDiagram>(result.value());
}

bool isBits(const QString& text)
{
    const auto result = DiagramParser::parse(text);
    EXPECT_TRUE(result.isSuccess()) << (result.isFailure() ? result.error().toStdString() : "");
    return result.isSuccess() && std::holds_alternative<BitsDiagram>(result.value());
}

QString parseError(const QString& text)
{
    const auto result = DiagramParser::parse(text);
    EXPECT_TRUE(result.isFailure());
    return result.isFailure() ? result.error() : QString();
}
}

TEST(DiagramParserTest, ASequenceDiagramStaysASequenceDiagram)
{
    EXPECT_TRUE(isSequence(QStringLiteral("A -> B : hi\n")));
    EXPECT_TRUE(isSequence(QStringLiteral("@startuml\ntitle T\nparticipant A\nA -> B\n@enduml\n")));
    EXPECT_TRUE(isSequence(QStringLiteral("skinparam shadowing false\nactor User\nUser -> B\n")));
}

TEST(DiagramParserTest, AnActivityIsRecognisedByItsFirstMeaningfulLine)
{
    EXPECT_TRUE(isActivity(QStringLiteral("start\nstop\n")));
    EXPECT_TRUE(isActivity(QStringLiteral(":Do it;\n")));
    EXPECT_TRUE(isActivity(QStringLiteral("#red:Do it;\n")));
    EXPECT_TRUE(isActivity(QStringLiteral("if (x?) then (yes)\n:a;\nendif\n")));
    EXPECT_TRUE(isActivity(QStringLiteral("while (x?) is (yes)\n:a;\nendwhile\n")));
    EXPECT_TRUE(isActivity(QStringLiteral("repeat\n:a;\nrepeat while (x?)\n")));
    EXPECT_TRUE(isActivity(QStringLiteral("repeat :a;\nrepeat while (x?)\n")));
    EXPECT_TRUE(isActivity(QStringLiteral("fork\n:a;\nfork again\n:b;\nend fork\n")));
    EXPECT_TRUE(isActivity(QStringLiteral("partition P {\n:a;\n}\n")));
}

TEST(DiagramParserTest, MarkersTitlesCommentsAndDirectivesDoNotDecideTheKind)
{
    EXPECT_TRUE(isActivity(QStringLiteral("@startuml\n"
                                          "' comment\n"
                                          "/' block '/\n"
                                          "title Flow\n"
                                          "skinparam monochrome true\n"
                                          "skinparam activity {\nColor red\n}\n"
                                          "!theme plain\n"
                                          "hide footbox\n"
                                          "legend\nstart\nendlegend\n"
                                          "\n"
                                          "start\n"
                                          "stop\n"
                                          "@enduml\n")));
}

TEST(DiagramParserTest, ParticipantsNamedLikeActivityKeywordsStayInSequenceDiagrams)
{
    EXPECT_TRUE(isSequence(QStringLiteral("Start -> Fork : go\n")));
    EXPECT_TRUE(isSequence(QStringLiteral("Repeat -> While : again\n")));
}

TEST(DiagramParserTest, UnsupportedActivityFormsAreStillRoutedToTheActivityParser)
{
    EXPECT_TRUE(parseError(QStringLiteral("|Customer|\nstart\n"))
                    .contains(QStringLiteral("does not support swimlanes")));
    EXPECT_TRUE(parseError(QStringLiteral("switch (kind?)\ncase (a)\n:x;\nendswitch\n"))
                    .contains(QStringLiteral("does not support switch")));
    EXPECT_TRUE(parseError(QStringLiteral("(*) --> \"First\"\n"))
                    .contains(QStringLiteral("does not support the old activity syntax")));
}

TEST(DiagramParserTest, ABlockDiagramIsRecognisedByItsFirstKeyword)
{
    EXPECT_TRUE(isBlock(QStringLiteral("rectangle \"A\" as a\n")));
    EXPECT_TRUE(isBlock(QStringLiteral("node \"A\" as a\n")));
    EXPECT_TRUE(isBlock(QStringLiteral("component \"A\" as a\n")));
    EXPECT_TRUE(isBlock(QStringLiteral("database \"A\" as a\n")));
    EXPECT_TRUE(isBlock(QStringLiteral("rectangle A\nrectangle B\nA -- B\n")));
    EXPECT_TRUE(isBlock(QStringLiteral("@startuml\n"
                                       "' comment\n"
                                       "title Board\n"
                                       "skinparam monochrome true\n"
                                       "rectangle \"SoC\" {\n"
                                       "  rectangle CPU\n"
                                       "}\n"
                                       "@enduml\n")));
}

TEST(DiagramParserTest, ParticipantsNamedLikeBlockKeywordsStayInSequenceDiagrams)
{
    EXPECT_TRUE(isSequence(QStringLiteral("Node -> Server : go\n")));
    EXPECT_TRUE(isSequence(QStringLiteral("Component -> Database : save\n")));
    EXPECT_TRUE(isSequence(QStringLiteral("participant Node\nNode -> B\n")));
    EXPECT_TRUE(isSequence(QStringLiteral("actor User\ndatabase DB\nUser -> DB\n")));
}

TEST(DiagramParserTest, ASequenceDiagramStartingWithADatabaseParticipantStaysSequence)
{
    EXPECT_TRUE(isSequence(QStringLiteral("database DB\nApp -> DB : query\n")));
    EXPECT_TRUE(isSequence(QStringLiteral("database DB\nApp ->> DB\nDB --> App\n")));
    EXPECT_TRUE(isSequence(QStringLiteral("database DB\nDB <- App\n")));
    EXPECT_TRUE(isSequence(QStringLiteral("database DB\nDB <<- App\n")));
    EXPECT_TRUE(isSequence(QStringLiteral("database DB\nparticipant App\nApp --> DB\n")));
    EXPECT_TRUE(isSequence(QStringLiteral("database DB\nactor App\n")));
    EXPECT_TRUE(isSequence(QStringLiteral("database DB\nentity E\n")));
    EXPECT_TRUE(isSequence(QStringLiteral("database DB\nactivate DB\n")));
    EXPECT_TRUE(isSequence(QStringLiteral("database DB\nloop forever\nend\n")));
    EXPECT_TRUE(isSequence(QStringLiteral("database DB\n== Phase ==\n")));
    EXPECT_TRUE(isSequence(QStringLiteral("database DB\nnote over DB : saved\n")));
    EXPECT_TRUE(isSequence(QStringLiteral("@startuml\n"
                                          "title Query\n"
                                          "database DB\n"
                                          "App -> DB : query\n"
                                          "@enduml\n")));
}

TEST(DiagramParserTest, ADatabaseFirstBlockDiagramWithoutSequenceConstructsIsABlock)
{
    EXPECT_TRUE(isBlock(QStringLiteral("database \"Store\" as S\n"
                                       "rectangle \"App\" as A\n"
                                       "A -- S\n"
                                       "A .. S : sync\n")));
    EXPECT_TRUE(isBlock(QStringLiteral("database \"Store\" as S\n"
                                       "rectangle \"App\" as A\n"
                                       "A --> S : reads -> fast\n"
                                       "note left of A\n"
                                       "talks -> to S\n"
                                       "end note\n")));
}

TEST(DiagramParserTest, UnsupportedBlockFormsAreRoutedToTheBlockParser)
{
    EXPECT_TRUE(parseError(QStringLiteral("package Drivers {\nrectangle A\n}\n"))
                    .contains(QStringLiteral("does not support \"package\" elements")));
    EXPECT_TRUE(parseError(QStringLiteral("interface Api\nrectangle A\n"))
                    .contains(QStringLiteral("does not support interfaces")));
    EXPECT_TRUE(parseError(QStringLiteral("[Component] as C\n"))
                    .contains(QStringLiteral("does not support the [Component] short syntax")));
    EXPECT_TRUE(parseError(QStringLiteral("rectangle A\ncloud Net\n"))
                    .contains(QStringLiteral("does not support \"cloud\" elements")));
}

TEST(DiagramParserTest, AStateDiagramIsRecognisedByItsFirstInstruction)
{
    EXPECT_TRUE(isState(QStringLiteral("[*] --> Ready\nReady --> [*]\n")));
    EXPECT_TRUE(isState(QStringLiteral("state Ready\nReady --> Running\n")));
    EXPECT_TRUE(isState(QStringLiteral("state \"Long name\" as Long\nLong --> Next\n")));
    EXPECT_TRUE(isState(QStringLiteral("@startuml\n"
                                       "' comment\n"
                                       "title Machine\n"
                                       "[*] --> Ready\n"
                                       "@enduml\n")));
}

TEST(DiagramParserTest, ATransitionAloneStaysASequenceDiagram)
{
    EXPECT_TRUE(isSequence(QStringLiteral("A --> B\nB --> A : back\n")));
    EXPECT_TRUE(isSequence(QStringLiteral("Ready -> Running\n")));
}

TEST(DiagramParserTest, UnsupportedStateFormsAreRoutedToTheStateParser)
{
    EXPECT_TRUE(parseError(QStringLiteral("[*] --> A\nA --> [H]\n"))
                    .contains(QStringLiteral("does not support history states")));
    EXPECT_TRUE(parseError(QStringLiteral("state Pick <<choice>>\n"))
                    .contains(QStringLiteral("does not support stereotypes")));
    EXPECT_TRUE(parseError(QStringLiteral("state A #red\n"))
                    .contains(QStringLiteral("does not support state colours")));
    EXPECT_TRUE(parseError(QStringLiteral("state M {\nstate A\n--\nstate B\n}\n"))
                    .contains(QStringLiteral("does not support concurrent regions")));
    EXPECT_TRUE(parseError(QStringLiteral("hide empty description\n[*] --> A\n"))
                    .contains(QStringLiteral("does not support \"hide\" and \"show\"")));
}

TEST(DiagramParserTest, AClassDiagramIsRecognisedByItsFirstInstruction)
{
    EXPECT_TRUE(isClass(QStringLiteral("class Task\nTask --> Mm\n")));
    EXPECT_TRUE(isClass(QStringLiteral("struct task_struct {\n  pid : int\n}\n")));
    EXPECT_TRUE(isClass(QStringLiteral("enum Color {\n  RED\n}\n")));
    EXPECT_TRUE(isClass(QStringLiteral("abstract class Shape\nShape <|-- Circle\n")));
    EXPECT_TRUE(isClass(QStringLiteral("class \"Long name\" as Long\nLong --> Next\n")));
    EXPECT_TRUE(isClass(QStringLiteral("@startuml\n"
                                       "' comment\n"
                                       "title Memory\n"
                                       "struct A\n"
                                       "@enduml\n")));
}

TEST(DiagramParserTest, AnInterfaceInsideAClassDiagramIsFine)
{
    EXPECT_TRUE(
        isClass(QStringLiteral("class Circle\ninterface Drawable\nDrawable <|.. Circle\n")));
}

TEST(DiagramParserTest, AnInterfaceAsTheFirstLineStillGoesToTheBlockParser)
{
    EXPECT_TRUE(parseError(QStringLiteral("interface Drawable\nclass Circle\n"))
                    .contains(QStringLiteral("does not support interfaces")));
}

TEST(DiagramParserTest, ParticipantsNamedLikeClassKeywordsStayInSequenceDiagrams)
{
    EXPECT_TRUE(isSequence(QStringLiteral("Class -> Struct : go\n")));
    EXPECT_TRUE(isSequence(QStringLiteral("Enum -> Class\n")));
}

TEST(DiagramParserTest, UnsupportedClassFormsAreRoutedToTheClassParser)
{
    EXPECT_TRUE(parseError(QStringLiteral("class A <<entity>>\n"))
                    .contains(QStringLiteral("does not support stereotypes")));
    EXPECT_TRUE(parseError(QStringLiteral("class A {\n  {static} x : int\n}\n"))
                    .contains(QStringLiteral("does not support member modifiers")));
    EXPECT_TRUE(parseError(QStringLiteral("class A\npackage p {\n}\n"))
                    .contains(QStringLiteral("does not support \"package\" and \"namespace\"")));
}

TEST(DiagramParserTest, ABitsDiagramIsRecognisedByItsLayoutLine)
{
    EXPECT_TRUE(isBits(QStringLiteral("bits 32\nfield 0..31 word\n")));
    EXPECT_TRUE(isBits(QStringLiteral("bytes 4\nfield 0..3 int id\n")));
    EXPECT_TRUE(isBits(QStringLiteral("@startbits\n"
                                      "' comment\n"
                                      "title Packet\n"
                                      "bits 8\n"
                                      "field 0..7 flags\n"
                                      "@endbits\n")));
    EXPECT_TRUE(isBits(QStringLiteral("@startuml\nbits 8\nfield 0..7 flags\n@enduml\n")));
}

TEST(DiagramParserTest, ANodeNamedLikeALayoutKeywordStaysASequenceMessage)
{
    EXPECT_TRUE(isSequence(QStringLiteral("Bits -> Bytes : go\n")));
    EXPECT_TRUE(isSequence(QStringLiteral("bits -> B\n")));
    EXPECT_TRUE(isSequence(QStringLiteral("participant bytes\nbytes -> A\n")));
}

TEST(DiagramParserTest, AFieldBeforeTheLayoutLineStillGoesToTheBitsParser)
{
    EXPECT_TRUE(parseError(QStringLiteral("field 0..7 flags\n"))
                    .contains(QStringLiteral("the layout is missing")));
    EXPECT_TRUE(parseError(QStringLiteral("bits 8\nwhatever\n"))
                    .contains(QStringLiteral("A bits diagram here understands")));
}

TEST(DiagramParserTest, ErrorsComeFromTheParserOfTheDetectedKind)
{
    EXPECT_TRUE(parseError(QStringLiteral("rectangle A\nwhatever\n"))
                    .contains(QStringLiteral("A block diagram here understands")));
    EXPECT_TRUE(parseError(QStringLiteral("start\nwhatever\n"))
                    .contains(QStringLiteral("An activity diagram here understands")));
    EXPECT_TRUE(parseError(QStringLiteral("A -> B\nwhatever\n"))
                    .contains(QStringLiteral("A sequence diagram here understands")));
    EXPECT_TRUE(parseError(QStringLiteral("[*] --> A\nwhatever\n"))
                    .contains(QStringLiteral("A state diagram here understands")));
    EXPECT_TRUE(parseError(QStringLiteral("class A\nwhatever\n"))
                    .contains(QStringLiteral("A class diagram here understands")));
}

TEST(DiagramParserTest, WarningsOfEitherKindAreReachableThroughOneFunction)
{
    const auto activity = DiagramParser::parse(QStringLiteral("skinparam x 1\nstart\n"));
    ASSERT_TRUE(activity.isSuccess());
    EXPECT_EQ(warningsOf(activity.value()).size(), 1);

    const auto sequence = DiagramParser::parse(QStringLiteral("autonumber\nA -> B\n"));
    ASSERT_TRUE(sequence.isSuccess());
    EXPECT_EQ(warningsOf(sequence.value()).size(), 1);

    const auto blocks = DiagramParser::parse(QStringLiteral("skinparam x 1\nrectangle A\n"));
    ASSERT_TRUE(blocks.isSuccess());
    EXPECT_EQ(warningsOf(blocks.value()).size(), 1);

    const auto states = DiagramParser::parse(QStringLiteral("[*] -up-> A\n"));
    ASSERT_TRUE(states.isSuccess());
    EXPECT_EQ(warningsOf(states.value()).size(), 1);

    const auto classes = DiagramParser::parse(QStringLiteral("scale 2\nclass A\n"));
    ASSERT_TRUE(classes.isSuccess());
    EXPECT_EQ(warningsOf(classes.value()).size(), 1);

    const auto bits = DiagramParser::parse(QStringLiteral("bits 8\nfield 0..7 flags\n"));
    ASSERT_TRUE(bits.isSuccess());
    EXPECT_TRUE(warningsOf(bits.value()).isEmpty());
}
