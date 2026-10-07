#include <gtest/gtest.h>

#include "diagram/sequenceparser.h"

using namespace diagram;

namespace
{
SequenceDiagram parsed(const QString& text)
{
    const auto result = SequenceParser::parse(text);
    EXPECT_TRUE(result.isSuccess()) << (result.isFailure() ? result.error().toStdString() : "");
    return result.isSuccess() ? result.value() : SequenceDiagram {};
}

QString parseError(const QString& text)
{
    const auto result = SequenceParser::parse(text);
    EXPECT_TRUE(result.isFailure());
    return result.isFailure() ? result.error() : QString();
}

Message message(int from, int to, const QString& label, LineStyle line = LineStyle::Solid,
                MessageHead head = MessageHead::Filled)
{
    return Message { from, to, label, line, head };
}
}

TEST(SequenceParserTest, ReadsDeclaredParticipantsInOrder)
{
    const auto diagram = parsed(QStringLiteral("@startuml\n"
                                               "participant Client\n"
                                               "participant \"Auth server\" as Auth\n"
                                               "actor User\n"
                                               "database DB\n"
                                               "@enduml\n"));

    const QList<Participant> expected {
        { QStringLiteral("Client"), QStringLiteral("Client"), ParticipantKind::Box },
        { QStringLiteral("Auth"), QStringLiteral("Auth server"), ParticipantKind::Box },
        { QStringLiteral("User"), QStringLiteral("User"), ParticipantKind::Actor },
        { QStringLiteral("DB"), QStringLiteral("DB"), ParticipantKind::Box },
    };
    EXPECT_EQ(diagram.participants, expected);
    EXPECT_TRUE(diagram.warnings.isEmpty());
}

TEST(SequenceParserTest, AcceptsTheAliasOnEitherSide)
{
    const auto diagram = parsed(QStringLiteral("participant Auth as \"Auth server\"\n"
                                               "participant Long as L\n"));

    ASSERT_EQ(diagram.participants.size(), 2);
    EXPECT_EQ(diagram.participants[0].id, QStringLiteral("Auth"));
    EXPECT_EQ(diagram.participants[0].label, QStringLiteral("Auth server"));
    EXPECT_EQ(diagram.participants[1].id, QStringLiteral("L"));
    EXPECT_EQ(diagram.participants[1].label, QStringLiteral("Long"));
}

TEST(SequenceParserTest, CreatesUndeclaredParticipantsOnFirstUse)
{
    const auto diagram = parsed(QStringLiteral("Client -> Server : hello\n"));

    ASSERT_EQ(diagram.participants.size(), 2);
    EXPECT_EQ(diagram.participants[0].id, QStringLiteral("Client"));
    EXPECT_EQ(diagram.participants[1].id, QStringLiteral("Server"));
    ASSERT_EQ(diagram.steps.size(), 1);
    EXPECT_EQ(std::get<Message>(diagram.steps[0]), message(0, 1, QStringLiteral("hello")));
}

TEST(SequenceParserTest, ALaterDeclarationRelabelsWithoutMoving)
{
    const auto diagram = parsed(QStringLiteral("A -> B\n"
                                               "actor A as \"Alice\"\n"));

    ASSERT_EQ(diagram.participants.size(), 2);
    EXPECT_EQ(diagram.participants[0].label, QStringLiteral("Alice"));
    EXPECT_EQ(diagram.participants[0].kind, ParticipantKind::Actor);
}

TEST(SequenceParserTest, ReadsEveryArrowForm)
{
    const auto diagram = parsed(QStringLiteral("A -> B : call\n"
                                               "B --> A : reply\n"
                                               "A ->> B : event\n"
                                               "A -->> B : late\n"
                                               "A <- B : back\n"
                                               "A <-- B : back reply\n"
                                               "A->B\n"
                                               "A -> A : self\n"));

    const QList<SequenceStep> expected {
        message(0, 1, QStringLiteral("call")),
        message(1, 0, QStringLiteral("reply"), LineStyle::Dashed),
        message(0, 1, QStringLiteral("event"), LineStyle::Solid, MessageHead::Open),
        message(0, 1, QStringLiteral("late"), LineStyle::Dashed, MessageHead::Open),
        message(1, 0, QStringLiteral("back")),
        message(1, 0, QStringLiteral("back reply"), LineStyle::Dashed),
        message(0, 1, QString()),
        message(0, 0, QStringLiteral("self")),
    };
    EXPECT_EQ(diagram.steps, expected);
}

TEST(SequenceParserTest, QuotedNamesAndLineBreaksInLabels)
{
    const auto diagram = parsed(QStringLiteral("\"Web app\" -> Api : GET /items\\n?page=2\n"));

    EXPECT_EQ(diagram.participants[0].id, QStringLiteral("Web app"));
    EXPECT_EQ(std::get<Message>(diagram.steps[0]).label, QStringLiteral("GET /items\n?page=2"));
}

TEST(SequenceParserTest, ALabelMayContainAnArrow)
{
    const auto diagram = parsed(QStringLiteral("A -> B : x -> y\n"));

    EXPECT_EQ(std::get<Message>(diagram.steps[0]).label, QStringLiteral("x -> y"));
}

TEST(SequenceParserTest, ReturnRepliesToThePreviousMessage)
{
    const auto diagram = parsed(QStringLiteral("A -> B : ask\n"
                                               "return answer\n"));

    EXPECT_EQ(std::get<Message>(diagram.steps[1]),
              message(1, 0, QStringLiteral("answer"), LineStyle::Dashed));
}

TEST(SequenceParserTest, ReadsNotesInEveryPlacement)
{
    const auto diagram = parsed(QStringLiteral("participant A\n"
                                               "participant B\n"
                                               "participant C\n"
                                               "note left of A : one\n"
                                               "note right of B : two\n"
                                               "note over C, A : three\n"
                                               "A -> C : go\n"
                                               "note right : four\n"
                                               "note over : five\n"));

    const QList<SequenceStep> expected {
        Note { 0, 0, NotePlacement::Left, QStringLiteral("one") },
        Note { 1, 1, NotePlacement::Right, QStringLiteral("two") },
        Note { 0, 2, NotePlacement::Over, QStringLiteral("three") },
        message(0, 2, QStringLiteral("go")),
        Note { 2, 2, NotePlacement::Right, QStringLiteral("four") },
        Note { 0, 2, NotePlacement::Over, QStringLiteral("five") },
    };
    EXPECT_EQ(diagram.steps, expected);
}

TEST(SequenceParserTest, ReadsAMultiLineNote)
{
    const auto diagram = parsed(QStringLiteral("note over A\n"
                                               "  first line\n"
                                               "  second line\n"
                                               "end note\n"));

    EXPECT_EQ(std::get<Note>(diagram.steps[0]).text, QStringLiteral("first line\nsecond line"));
}

TEST(SequenceParserTest, ReadsDividersAndDelays)
{
    const auto diagram = parsed(QStringLiteral("A -> B\n"
                                               "== Later ==\n"
                                               "...\n"
                                               "... 5 minutes ...\n"));

    EXPECT_EQ(std::get<Divider>(diagram.steps[1]).label, QStringLiteral("Later"));
    EXPECT_EQ(std::get<Divider>(diagram.steps[2]).label, QString());
    EXPECT_EQ(std::get<Divider>(diagram.steps[3]).label, QStringLiteral("5 minutes"));
}

TEST(SequenceParserTest, ReadsNestedGroups)
{
    const auto diagram = parsed(QStringLiteral("alt valid\n"
                                               "  A -> B : ok\n"
                                               "  loop 3 times\n"
                                               "    A -> B : retry\n"
                                               "  end\n"
                                               "else invalid\n"
                                               "  B --> A : error\n"
                                               "end\n"));

    const QList<SequenceStep> expected {
        GroupStart { QStringLiteral("alt"), QStringLiteral("valid") },
        message(0, 1, QStringLiteral("ok")),
        GroupStart { QStringLiteral("loop"), QStringLiteral("3 times") },
        message(0, 1, QStringLiteral("retry")),
        GroupEnd {},
        GroupElse { QStringLiteral("invalid") },
        message(1, 0, QStringLiteral("error"), LineStyle::Dashed),
        GroupEnd {},
    };
    EXPECT_EQ(diagram.steps, expected);
}

TEST(SequenceParserTest, ReadsEveryGroupKeyword)
{
    const auto diagram = parsed(QStringLiteral("A -> B\n"
                                               "opt\nend\npar\nend\nbreak\nend\n"
                                               "critical\nend alt\nGroup My label\nend group\n"));

    EXPECT_EQ(std::get<GroupStart>(diagram.steps[1]).keyword, QStringLiteral("opt"));
    EXPECT_EQ(std::get<GroupStart>(diagram.steps[3]).keyword, QStringLiteral("par"));
    EXPECT_EQ(std::get<GroupStart>(diagram.steps[5]).keyword, QStringLiteral("break"));
    EXPECT_EQ(std::get<GroupStart>(diagram.steps[7]).keyword, QStringLiteral("critical"));
    EXPECT_EQ(std::get<GroupStart>(diagram.steps[9]),
              (GroupStart { QStringLiteral("group"), QStringLiteral("My label") }));
    EXPECT_EQ(diagram.steps.size(), 11);
}

TEST(SequenceParserTest, ReturnsUnwindNestedCalls)
{
    const auto diagram = parsed(QStringLiteral("A -> B : one\n"
                                               "B -> C : two\n"
                                               "return two done\n"
                                               "return one done\n"));

    EXPECT_EQ(std::get<Message>(diagram.steps[2]),
              message(2, 1, QStringLiteral("two done"), LineStyle::Dashed));
    EXPECT_EQ(std::get<Message>(diagram.steps[3]),
              message(1, 0, QStringLiteral("one done"), LineStyle::Dashed));
}

TEST(SequenceParserTest, AnExplicitReplyClosesItsCall)
{
    const auto diagram = parsed(QStringLiteral("A -> B : one\n"
                                               "B -> C : two\n"
                                               "C --> B : two done\n"
                                               "return one done\n"));

    EXPECT_EQ(std::get<Message>(diagram.steps[3]),
              message(1, 0, QStringLiteral("one done"), LineStyle::Dashed));
}

TEST(SequenceParserTest, IgnoresArrowColoursWithAWarning)
{
    const auto diagram = parsed(QStringLiteral("A -[#red]> B : hot\n"
                                               "B -[#blue]-> A : cold\n"));

    EXPECT_EQ(std::get<Message>(diagram.steps[0]), message(0, 1, QStringLiteral("hot")));
    EXPECT_EQ(std::get<Message>(diagram.steps[1]),
              message(1, 0, QStringLiteral("cold"), LineStyle::Dashed));
    EXPECT_EQ(diagram.warnings.size(), 2);
}

TEST(SequenceParserTest, LineBreaksInParticipantLabels)
{
    const auto diagram = parsed(QStringLiteral("participant \"Web\\nServer\" as W\n"));

    EXPECT_EQ(diagram.participants[0].label, QStringLiteral("Web\nServer"));
}

TEST(SequenceParserTest, IgnoresStereotypes)
{
    const auto diagram = parsed(QStringLiteral("participant Api <<Service>>\n"));

    EXPECT_EQ(diagram.participants[0].label, QStringLiteral("Api"));
    EXPECT_EQ(diagram.warnings.size(), 1);
}

TEST(SequenceParserTest, ReadsLongerDividersAndOpenDelays)
{
    const auto diagram = parsed(QStringLiteral("A -> B\n=== Phase ===\n... later\n"));

    EXPECT_EQ(std::get<Divider>(diagram.steps[1]).label, QStringLiteral("Phase"));
    EXPECT_EQ(std::get<Divider>(diagram.steps[2]).label, QStringLiteral("later"));
}

TEST(SequenceParserTest, KeywordsCanNameParticipantsInMessages)
{
    const auto diagram = parsed(QStringLiteral("title -> note : x\n"
                                               "legend -> end : y\n"
                                               "alt -> return : z\n"));

    EXPECT_EQ(diagram.title, QString());
    EXPECT_EQ(diagram.participants.size(), 6);
    EXPECT_EQ(diagram.steps.size(), 3);
}

TEST(SequenceParserTest, ReadsNonAsciiNamesAndActivationWithoutSpace)
{
    const auto diagram = parsed(QStringLiteral("Użytkownik -> Serwer.v2++ : żądanie\n"));

    EXPECT_EQ(diagram.participants[0].id, QStringLiteral("Użytkownik"));
    EXPECT_EQ(diagram.participants[1].id, QStringLiteral("Serwer.v2"));
    EXPECT_EQ(std::get<Message>(diagram.steps[0]).label, QStringLiteral("żądanie"));
}

TEST(SequenceParserTest, StripsAByteOrderMarkAndTypographicQuotes)
{
    const auto diagram = parsed(QString(QChar(0xFEFF))
                                + QStringLiteral("@startuml\nparticipant “Web app” as W\n"));

    EXPECT_EQ(diagram.participants[0].label, QStringLiteral("Web app"));
}

TEST(SequenceParserTest, SkipsNestedAndSingleLineBlocks)
{
    const auto diagram = parsed(QStringLiteral("skinparam sequence {\n"
                                               "  participant {\n"
                                               "    BackgroundColor red\n"
                                               "  }\n"
                                               "}\n"
                                               "<style> root { } </style>\n"
                                               "legend top endlegend\n"
                                               "A -> B\n"));

    EXPECT_EQ(diagram.steps.size(), 1);
    EXPECT_EQ(diagram.warnings.size(), 3);
}

TEST(SequenceParserTest, ANoteMayBeEmpty)
{
    const auto diagram = parsed(QStringLiteral("note over A :\n"));

    EXPECT_EQ(std::get<Note>(diagram.steps[0]).text, QString());
}

TEST(SequenceParserTest, SkipsCommentsAndBlankLines)
{
    const auto diagram = parsed(QStringLiteral("' a comment\n"
                                               "\n"
                                               "/' a block\n"
                                               "   comment '/\n"
                                               "/' one line '/\n"
                                               "A -> B\n"));

    EXPECT_EQ(diagram.steps.size(), 1);
    EXPECT_TRUE(diagram.warnings.isEmpty());
}

TEST(SequenceParserTest, IgnoresDecorationWithAWarningPerLine)
{
    const auto diagram = parsed(QStringLiteral("skinparam monochrome true\n"
                                               "hide footbox\n"
                                               "!theme plain\n"
                                               "autonumber\n"
                                               "participant A #red\n"
                                               "A -> B ++ : call\n"
                                               "activate B\n"
                                               "deactivate B\n"
                                               "|||\n"));

    EXPECT_EQ(diagram.steps.size(), 1);
    EXPECT_EQ(diagram.warnings.size(), 9);
    EXPECT_EQ(diagram.warnings.first(), QStringLiteral("Line 1: \"skinparam\" ignored"));
    EXPECT_EQ(diagram.warnings.at(4), QStringLiteral("Line 5: colour ignored"));
}

TEST(SequenceParserTest, SkipsWholeDecorationBlocks)
{
    const auto diagram = parsed(QStringLiteral("skinparam sequence {\n"
                                               "  ArrowColor red\n"
                                               "}\n"
                                               "legend right\n"
                                               "  A -> B\n"
                                               "endlegend\n"
                                               "box \"Inside\"\n"
                                               "participant A\n"
                                               "end box\n"
                                               "A -> B\n"));

    EXPECT_EQ(diagram.steps.size(), 1);
    EXPECT_EQ(diagram.warnings.size(), 3);
}

TEST(SequenceParserTest, KeywordsAreCaseInsensitive)
{
    const auto diagram = parsed(QStringLiteral("Participant A\nALT x\nA -> A\nEND\n"));

    EXPECT_EQ(diagram.steps.size(), 3);
}

TEST(SequenceParserTest, ReadsATitle)
{
    EXPECT_EQ(parsed(QStringLiteral("title Login flow\nA -> B\n")).title,
              QStringLiteral("Login flow"));
}

TEST(SequenceParserTest, ReportsAnUnknownStatementWithItsLine)
{
    EXPECT_EQ(parseError(QStringLiteral("A -> B\nwhatever\n")),
              QStringLiteral("Line 2: unknown statement \"whatever\""));
}

TEST(SequenceParserTest, HintsHowToFixCommonMistakes)
{
    EXPECT_TRUE(
        parseError(QStringLiteral("A <-> B\n")).endsWith(QStringLiteral("write two messages")));
    EXPECT_TRUE(
        parseError(QStringLiteral("[-> A : x\n")).contains(QStringLiteral("name the sender")));
    EXPECT_TRUE(
        parseError(QStringLiteral("A ->] : x\n")).contains(QStringLiteral("name the sender")));
    EXPECT_TRUE(
        parseError(QStringLiteral("ref over A, B : x\n")).contains(QStringLiteral("note over")));
    EXPECT_TRUE(parseError(QStringLiteral("Web Browser -> Server : x\n"))
                    .contains(QStringLiteral("must be quoted")));
}

TEST(SequenceParserTest, ReportsAnArrowWithoutATarget)
{
    EXPECT_TRUE(parseError(QStringLiteral("A ->\n")).startsWith(QStringLiteral("Line 1:")));
}

TEST(SequenceParserTest, ReportsUnbalancedGroups)
{
    EXPECT_EQ(parseError(QStringLiteral("A -> B\nend\n")),
              QStringLiteral("Line 2: \"end\" without an open group"));
    EXPECT_EQ(parseError(QStringLiteral("A -> B\nelse\n")),
              QStringLiteral("Line 2: \"else\" outside of a group"));
    EXPECT_EQ(parseError(QStringLiteral("alt x\nA -> B\nloop\nend\n")),
              QStringLiteral("Line 1: \"alt\" is not closed with \"end\""));
}

TEST(SequenceParserTest, ReportsStatementsThatNeedAPreviousMessage)
{
    EXPECT_EQ(parseError(QStringLiteral("participant A\nreturn x\n")),
              QStringLiteral("Line 2: \"return\" has no call to return from"));
    EXPECT_EQ(parseError(QStringLiteral("A -> B\nreturn x\nreturn y\n")),
              QStringLiteral("Line 3: \"return\" has no call to return from"));
    EXPECT_EQ(parseError(QStringLiteral("participant A\nnote right : x\n")),
              QStringLiteral("Line 2: a note without a participant needs a message before it"));
}

TEST(SequenceParserTest, ReportsUnclosedBlocks)
{
    EXPECT_EQ(parseError(QStringLiteral("A -> B\nnote over A\ntext\n")),
              QStringLiteral("Line 2: note is not closed with \"end note\""));
    EXPECT_EQ(parseError(QStringLiteral("A -> B\nskinparam x {\n")),
              QStringLiteral("Line 2: \"skinparam\" block is not closed"));
    EXPECT_EQ(parseError(QStringLiteral("A -> B\nlegend\nx\n")),
              QStringLiteral("Line 2: \"legend\" block is not closed"));
    EXPECT_EQ(parseError(QStringLiteral("A -> B\n<style>\nx\n")),
              QStringLiteral("Line 2: \"style\" block is not closed"));
}

TEST(SequenceParserTest, ReportsANoteSideThatSpansParticipants)
{
    EXPECT_EQ(parseError(QStringLiteral("note left of A, B : x\n")),
              QStringLiteral("Line 1: only \"note over\" can span participants"));
}

TEST(SequenceParserTest, ReportsAnEmptyDiagram)
{
    EXPECT_EQ(parseError(QStringLiteral("@startuml\n' nothing\n@enduml\n")),
              QStringLiteral("The diagram has no participants"));
}

TEST(SequenceParserTest, RejectsAnOversizedDiagram)
{
    EXPECT_EQ(parseError(QStringLiteral("A -> B : x\n").repeated(2000)),
              QStringLiteral("The diagram is longer than 20000 characters"));
}

TEST(SequenceParserTest, RejectsTooManySteps)
{
    EXPECT_EQ(parsed(QStringLiteral("A -> B\n").repeated(300)).steps.size(), 300);
    EXPECT_EQ(parseError(QStringLiteral("A -> B\n").repeated(301)),
              QStringLiteral("Line 301: the diagram has more than 300 steps"));
}

TEST(SequenceParserTest, AcceptsWindowsLineEndings)
{
    EXPECT_EQ(parsed(QStringLiteral("A -> B : x\r\nB --> A : y\r\n")).steps.size(), 2);
}
