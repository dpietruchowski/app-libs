#include <gtest/gtest.h>

#include "diagram/stateparser.h"

using namespace diagram;

namespace
{
const QString kLifeCycle = QStringLiteral("@startuml\n"
                                          "[*] --> Ready\n"
                                          "Ready --> Running : scheduled\n"
                                          "Running --> Ready : preempted\n"
                                          "Running --> Blocked : waits for I/O\n"
                                          "Blocked --> Ready : I/O done\n"
                                          "Running --> [*] : exit\n"
                                          "@enduml\n");

StateDiagram parsed(const QString& text)
{
    const auto result = StateParser::parse(text);
    EXPECT_TRUE(result.isSuccess()) << (result.isFailure() ? result.error().toStdString() : "");
    return result.isSuccess() ? result.value() : StateDiagram {};
}

QString parseError(const QString& text)
{
    const auto result = StateParser::parse(text);
    EXPECT_TRUE(result.isFailure());
    return result.isFailure() ? result.error() : QString();
}

int indexOfState(const StateDiagram& diagram, const QString& id)
{
    for (qsizetype index = 0; index < diagram.states.size(); ++index)
    {
        if (diagram.states.at(index).id == id)
            return int(index);
    }
    return -1;
}

void expectUnsupported(const QString& text, const QString& feature)
{
    const QString error = parseError(text);
    EXPECT_TRUE(error.contains(QStringLiteral("is valid PlantUML, but this app does not support ")
                               + feature))
        << error.toStdString();
    EXPECT_TRUE(error.contains(QStringLiteral("A state diagram here understands: ")))
        << error.toStdString();
}
}

TEST(StateParserTest, ReadsStatesTransitionsLabelsAndPseudoStates)
{
    const auto diagram = parsed(kLifeCycle);

    ASSERT_EQ(diagram.states.size(), 5);
    EXPECT_EQ(diagram.states.at(0).kind, StateKind::Initial);
    EXPECT_EQ(diagram.states.at(1).id, QStringLiteral("Ready"));
    EXPECT_EQ(diagram.states.at(1).label, QStringLiteral("Ready"));
    EXPECT_EQ(diagram.states.at(4).kind, StateKind::Final);

    ASSERT_EQ(diagram.transitions.size(), 6);
    EXPECT_EQ(diagram.transitions.at(0).from, 0);
    EXPECT_EQ(diagram.transitions.at(0).to, 1);
    EXPECT_TRUE(diagram.transitions.at(0).label.isEmpty());
    EXPECT_EQ(diagram.transitions.at(2).label, QStringLiteral("preempted"));
    EXPECT_EQ(diagram.transitions.at(5).to, 4);
    EXPECT_EQ(diagram.transitions.at(5).sourceLine, 7);
    EXPECT_TRUE(diagram.warnings.isEmpty());
}

TEST(StateParserTest, ReadsTheShortArrowAndAQuotedState)
{
    const auto diagram = parsed(QStringLiteral("state \"Waiting for input\" as Wait\n"
                                               "[*] -> Wait\n"
                                               "Wait -> \"Done\"\n"));

    ASSERT_EQ(diagram.states.size(), 3);
    EXPECT_EQ(diagram.states.at(0).id, QStringLiteral("Wait"));
    EXPECT_EQ(diagram.states.at(0).label, QStringLiteral("Waiting for input"));
    EXPECT_EQ(diagram.states.at(2).id, QStringLiteral("Done"));
    ASSERT_EQ(diagram.transitions.size(), 2);
    EXPECT_EQ(diagram.transitions.at(1).to, 2);
}

TEST(StateParserTest, ReadsAStateDeclaredWithTheAliasFirst)
{
    const auto diagram = parsed(QStringLiteral("state S1 as \"First\\nstate\"\n[*] --> S1\n"));

    ASSERT_EQ(diagram.states.size(), 2);
    EXPECT_EQ(diagram.states.at(0).id, QStringLiteral("S1"));
    EXPECT_EQ(diagram.states.at(0).label, QStringLiteral("First\nstate"));
}

TEST(StateParserTest, ReadsDescriptionLines)
{
    const auto diagram = parsed(QStringLiteral("state Idle\n"
                                               "Idle : waits for input\n"
                                               "Idle : no work queued\n"
                                               "Idle --> Busy\n"));

    ASSERT_EQ(diagram.states.size(), 2);
    EXPECT_EQ(
        diagram.states.at(0).descriptions,
        (QStringList { QStringLiteral("waits for input"), QStringLiteral("no work queued") }));
    EXPECT_EQ(diagram.transitions.size(), 1);
}

TEST(StateParserTest, KeepsAStateTransitionApartFromADescription)
{
    const auto diagram = parsed(QStringLiteral("A --> B : text\nB : more\n"));

    EXPECT_EQ(diagram.transitions.size(), 1);
    EXPECT_TRUE(diagram.states.at(0).descriptions.isEmpty());
    EXPECT_EQ(diagram.states.at(1).descriptions, QStringList { QStringLiteral("more") });
}

TEST(StateParserTest, NestsCompositeStatesTwoLevelsDeepWithTheirOwnPseudoStates)
{
    const auto diagram = parsed(QStringLiteral("[*] --> Machine\n"
                                               "state Machine {\n"
                                               "  [*] --> Inner\n"
                                               "  state Inner {\n"
                                               "    [*] --> Deep\n"
                                               "    Deep --> [*]\n"
                                               "  }\n"
                                               "  Inner --> [*]\n"
                                               "}\n"
                                               "Machine --> Off\n"));

    const int machine = indexOfState(diagram, QStringLiteral("Machine"));
    const int inner = indexOfState(diagram, QStringLiteral("Inner"));
    const int deep = indexOfState(diagram, QStringLiteral("Deep"));
    const int off = indexOfState(diagram, QStringLiteral("Off"));
    EXPECT_TRUE(diagram.states.at(machine).isComposite);
    EXPECT_TRUE(diagram.states.at(inner).isComposite);
    EXPECT_EQ(diagram.states.at(machine).parent, -1);
    EXPECT_EQ(diagram.states.at(inner).parent, machine);
    EXPECT_EQ(diagram.states.at(deep).parent, inner);
    EXPECT_EQ(diagram.states.at(off).parent, -1);

    int initials = 0;
    int finals = 0;
    for (const State& state : diagram.states)
    {
        initials += state.kind == StateKind::Initial;
        finals += state.kind == StateKind::Final;
    }
    EXPECT_EQ(initials, 3);
    EXPECT_EQ(finals, 2);
}

TEST(StateParserTest, ReadsASelfTransition)
{
    const auto diagram = parsed(QStringLiteral("Waiting --> Waiting : tick\n"));

    ASSERT_EQ(diagram.states.size(), 1);
    ASSERT_EQ(diagram.transitions.size(), 1);
    EXPECT_EQ(diagram.transitions.at(0).from, 0);
    EXPECT_EQ(diagram.transitions.at(0).to, 0);
}

TEST(StateParserTest, IgnoresDirectionHintsWithAWarning)
{
    const auto diagram = parsed(QStringLiteral("A -up-> B\nB -d-> C\nC --> A\n"));

    EXPECT_EQ(diagram.transitions.size(), 3);
    EXPECT_EQ(diagram.warnings,
              (QStringList { QStringLiteral("Line 1: direction hint ignored"),
                             QStringLiteral("Line 2: direction hint ignored") }));
}

TEST(StateParserTest, ReadsTitlesAndNotesOnBothSides)
{
    const auto diagram = parsed(QStringLiteral("title Traffic light\n"
                                               "Red --> Green\n"
                                               "note left of Red : stop\n"
                                               "note right of Green\n"
                                               "go\n"
                                               "now\n"
                                               "end note\n"));

    EXPECT_EQ(diagram.title, QStringLiteral("Traffic light"));
    ASSERT_EQ(diagram.notes.size(), 2);
    EXPECT_EQ(diagram.notes.at(0).state, 0);
    EXPECT_EQ(diagram.notes.at(0).side, StateNoteSide::Left);
    EXPECT_EQ(diagram.notes.at(0).text, QStringLiteral("stop"));
    EXPECT_EQ(diagram.notes.at(1).side, StateNoteSide::Right);
    EXPECT_EQ(diagram.notes.at(1).text, QStringLiteral("go\nnow"));
}

TEST(StateParserTest, ReportsAnEmptyDiagramWithoutALine)
{
    EXPECT_EQ(parseError(QStringLiteral("@startuml\ntitle Nothing\n@enduml\n")),
              QStringLiteral("The diagram has no states"));
}

TEST(StateParserTest, ReportsAnUnknownStatementAtItsLine)
{
    const QString error = parseError(QStringLiteral("[*] --> A\nwhatever now\n"));

    EXPECT_TRUE(error.startsWith(QStringLiteral("Line 2: unknown statement \"whatever now\". A "
                                                "state diagram here understands: ")));
}

TEST(StateParserTest, NamesTheFormOfAKnownKeywordWrittenWrongly)
{
    EXPECT_EQ(parseError(QStringLiteral("A --> B\nstate foo-bar\n")),
              QStringLiteral("Line 2: \"state foo-bar\" is not a valid \"state\" statement. Write "
                             "it as: state \"Long name\" as Alias, or state Name { to open a "
                             "composite state."));
}

TEST(StateParserTest, ReportsAnUnclosedCompositeAtTheLastLine)
{
    EXPECT_EQ(parseError(QStringLiteral("state Machine {\n  A --> B\n")),
              QStringLiteral("Line 2: the composite state \"Machine\" opened at line 1 is not "
                             "closed with \"}\". Add \"}\" where the composite state ends."));
}

TEST(StateParserTest, ReportsACloserWithoutAComposite)
{
    EXPECT_EQ(parseError(QStringLiteral("A --> B\n}\n")),
              QStringLiteral("Line 2: \"}\" without an open composite state"));
}

TEST(StateParserTest, ReportsADuplicateDeclarationAndAnUnknownNoteTarget)
{
    EXPECT_EQ(parseError(QStringLiteral("state A\nstate A\n")),
              QStringLiteral("Line 2: the state \"A\" is already declared at line 1. Give each "
                             "state its own name."));
    EXPECT_EQ(parseError(QStringLiteral("state A\nnote left of B : x\n")),
              QStringLiteral("Line 2: unknown state \"B\" in the note. Declare it before the "
                             "note."));
    EXPECT_EQ(parseError(QStringLiteral("state A\nnote left of A : x\nnote left of A : y\n")),
              QStringLiteral("Line 3: the state \"A\" already has a note on the left. Merge the "
                             "two notes into one."));
}

TEST(StateParserTest, ReportsATransitionBetweenTwoPseudoStates)
{
    EXPECT_TRUE(parseError(QStringLiteral("[*] --> [*]\n"))
                    .startsWith(QStringLiteral("Line 1: the transition \"[*] --> [*]\" goes from "
                                               "[*] to [*].")));
}

TEST(StateParserTest, ReportsANoteThatIsNotClosed)
{
    EXPECT_EQ(parseError(QStringLiteral("A --> B\nnote right of A\ntext\n")),
              QStringLiteral("Line 3: the note started at line 2 has no closing \"end note\". Add "
                             "\"end note\" after the text."));
}

TEST(StateParserTest, RejectsConcurrentRegions)
{
    expectUnsupported(QStringLiteral("state Machine {\n  A --> B\n  --\n  C --> D\n}\n"),
                      QStringLiteral("concurrent regions"));
    expectUnsupported(QStringLiteral("state Machine {\n  A --> B\n  ||\n  C --> D\n}\n"),
                      QStringLiteral("concurrent regions"));
}

TEST(StateParserTest, RejectsHistoryStates)
{
    expectUnsupported(QStringLiteral("A --> B\nB --> [H]\n"), QStringLiteral("history states"));
    expectUnsupported(QStringLiteral("A --> B\nB --> [H*]\n"), QStringLiteral("history states"));
}

TEST(StateParserTest, RejectsStereotypes)
{
    expectUnsupported(QStringLiteral("state Pick <<choice>>\n"),
                      QStringLiteral("stereotypes such as <<choice>>"));
    expectUnsupported(QStringLiteral("state Split <<fork>>\n"),
                      QStringLiteral("stereotypes such as <<choice>>"));
}

TEST(StateParserTest, RejectsStateColours)
{
    expectUnsupported(QStringLiteral("state Busy #red\n"), QStringLiteral("state colours"));
    expectUnsupported(QStringLiteral("state \"Busy now\" as Busy #lightblue\n"),
                      QStringLiteral("state colours"));
}

TEST(StateParserTest, RejectsStyleDirectivesAndLayoutDirections)
{
    expectUnsupported(QStringLiteral("skinparam monochrome true\nA --> B\n"),
                      QStringLiteral("skinparam"));
    expectUnsupported(QStringLiteral("hide empty description\nA --> B\n"),
                      QStringLiteral("\"hide\" and \"show\""));
    expectUnsupported(QStringLiteral("left to right direction\nA --> B\n"),
                      QStringLiteral("layout directions"));
}

TEST(StateParserTest, RejectsNotesAndArrowsThatAreNotDrawn)
{
    expectUnsupported(QStringLiteral("A --> B\nnote top of A : x\n"),
                      QStringLiteral("notes above or below a state"));
    expectUnsupported(QStringLiteral("A --> B\nfloating note : x\n"),
                      QStringLiteral("floating notes and notes on transitions"));
    expectUnsupported(QStringLiteral("A --> B\nA ..> B\n"), QStringLiteral("this kind of arrow"));
}

TEST(StateParserTest, ReportsTheLineOfAnUnsupportedElement)
{
    EXPECT_TRUE(parseError(QStringLiteral("[*] --> A\nA --> B\nB --> [H]\n"))
                    .startsWith(QStringLiteral("Line 3: \"B --> [H]\" is valid PlantUML, but this "
                                               "app does not support history states.")));
}
