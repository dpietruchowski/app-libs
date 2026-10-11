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

TEST(DiagramParserTest, ErrorsComeFromTheParserOfTheDetectedKind)
{
    EXPECT_TRUE(parseError(QStringLiteral("rectangle A\nwhatever\n"))
                    .contains(QStringLiteral("A block diagram here understands")));
    EXPECT_TRUE(parseError(QStringLiteral("start\nwhatever\n"))
                    .contains(QStringLiteral("An activity diagram here understands")));
    EXPECT_TRUE(parseError(QStringLiteral("A -> B\nwhatever\n"))
                    .contains(QStringLiteral("A sequence diagram here understands")));
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
}
