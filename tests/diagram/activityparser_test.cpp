#include <gtest/gtest.h>

#include "diagram/activityparser.h"

using namespace diagram;

namespace
{
ActivityDiagram parsed(const QString& text)
{
    const auto result = ActivityParser::parse(text);
    EXPECT_TRUE(result.isSuccess()) << (result.isFailure() ? result.error().toStdString() : "");
    return result.isSuccess() ? result.value() : ActivityDiagram {};
}

QString parseError(const QString& text)
{
    const auto result = ActivityParser::parse(text);
    EXPECT_TRUE(result.isFailure());
    return result.isFailure() ? result.error() : QString();
}

template <typename T> const T& nodeAt(const ActivityBlock& block, std::size_t index)
{
    EXPECT_LT(index, block.size());
    EXPECT_TRUE(std::holds_alternative<T>(block.at(index).node));
    static const T fallback {};
    return std::holds_alternative<T>(block.at(index).node) ? std::get<T>(block.at(index).node)
                                                           : fallback;
}

QString unsupportedText(const QString& line, const QString& feature, const QString& rewrite)
{
    return QStringLiteral("\"%1\" is valid PlantUML, but this app does not support %2. %3.")
        .arg(line, feature, rewrite);
}
}

TEST(ActivityParserTest, ReadsStartActionsAndEnds)
{
    const auto diagram = parsed(QStringLiteral("@startuml\n"
                                               "title Order flow\n"
                                               "start\n"
                                               ":Take the order;\n"
                                               ":Ship it;\n"
                                               "stop\n"
                                               "end\n"
                                               "@enduml\n"));

    EXPECT_EQ(diagram.title, QStringLiteral("Order flow"));
    ASSERT_EQ(diagram.body.size(), 5u);
    nodeAt<ActivityStart>(diagram.body, 0);
    EXPECT_EQ(nodeAt<ActivityAction>(diagram.body, 1).text, QStringLiteral("Take the order"));
    EXPECT_EQ(diagram.body.at(1).line, 4);
    EXPECT_EQ(nodeAt<ActivityAction>(diagram.body, 2).text, QStringLiteral("Ship it"));
    EXPECT_EQ(nodeAt<ActivityStop>(diagram.body, 3).kind, FlowEnd::Stop);
    EXPECT_EQ(nodeAt<ActivityStop>(diagram.body, 4).kind, FlowEnd::End);
    EXPECT_TRUE(diagram.warnings.isEmpty());
}

TEST(ActivityParserTest, ReadsAMultiLineActionAndLiteralLineBreaks)
{
    const auto diagram = parsed(QStringLiteral("start\n"
                                               ":first line\n"
                                               "second line;\n"
                                               ":one\\ntwo;\n"));

    EXPECT_EQ(nodeAt<ActivityAction>(diagram.body, 1).text,
              QStringLiteral("first line\nsecond line"));
    EXPECT_EQ(diagram.body.at(1).line, 2);
    EXPECT_EQ(nodeAt<ActivityAction>(diagram.body, 2).text, QStringLiteral("one\ntwo"));
}

TEST(ActivityParserTest, DrawsSdlShapesAsPlainActionsWithAWarning)
{
    const auto diagram = parsed(QStringLiteral("start\n:input>\n:output<\n:call|\n:data/\n"));

    ASSERT_EQ(diagram.body.size(), 5u);
    EXPECT_EQ(nodeAt<ActivityAction>(diagram.body, 1).text, QStringLiteral("input"));
    EXPECT_EQ(nodeAt<ActivityAction>(diagram.body, 4).text, QStringLiteral("data"));
    ASSERT_EQ(diagram.warnings.size(), 4);
    EXPECT_EQ(diagram.warnings.first(),
              QStringLiteral("Line 2: SDL shape \">\" drawn as a plain action"));
}

TEST(ActivityParserTest, IgnoresActionColoursWithAWarning)
{
    const auto diagram = parsed(QStringLiteral("start\n#palegreen:Do it;\n"));

    EXPECT_EQ(nodeAt<ActivityAction>(diagram.body, 1).text, QStringLiteral("Do it"));
    EXPECT_EQ(diagram.warnings, QStringList { QStringLiteral("Line 2: colour ignored") });
}

TEST(ActivityParserTest, KeepsConnectorLabelsOnTheNextElement)
{
    const auto diagram = parsed(QStringLiteral("start\n-> first label;\n:a;\n-[#red]-> second;\n"
                                               "if (x) then (y)\n:b;\nendif\n"));

    EXPECT_EQ(diagram.body.at(1).incomingLabel, QStringLiteral("first label"));
    EXPECT_EQ(diagram.body.at(2).incomingLabel, QStringLiteral("second"));
    EXPECT_EQ(diagram.warnings, QStringList { QStringLiteral("Line 4: arrow colour ignored") });
}

TEST(ActivityParserTest, DropsAConnectorLabelWithoutATargetWithAWarning)
{
    const auto diagram = parsed(QStringLiteral("start\n-> nowhere;\n"));

    EXPECT_EQ(diagram.warnings,
              QStringList { QStringLiteral(
                  "Line 2: connector label \"nowhere\" has no element after it, ignored") });
}

TEST(ActivityParserTest, ReadsIfElseifAndElse)
{
    const auto diagram = parsed(QStringLiteral("start\n"
                                               "if (a?) then (yes)\n"
                                               ":one;\n"
                                               "elseif (b?) then (maybe)\n"
                                               ":two;\n"
                                               "else (no)\n"
                                               ":three;\n"
                                               "endif\n"
                                               "stop\n"));

    const auto& branches = nodeAt<ActivityIf>(diagram.body, 1).branches;
    ASSERT_EQ(branches.size(), 3u);
    EXPECT_EQ(branches[0].condition, QStringLiteral("a?"));
    EXPECT_EQ(branches[0].guard, QStringLiteral("yes"));
    EXPECT_FALSE(branches[0].isElse);
    EXPECT_EQ(branches[1].condition, QStringLiteral("b?"));
    EXPECT_EQ(branches[1].guard, QStringLiteral("maybe"));
    EXPECT_TRUE(branches[2].isElse);
    EXPECT_EQ(branches[2].guard, QStringLiteral("no"));
    EXPECT_EQ(nodeAt<ActivityAction>(branches[2].block, 0).text, QStringLiteral("three"));
    EXPECT_EQ(diagram.body.at(1).line, 2);
}

TEST(ActivityParserTest, ReadsAnIfWithoutThenLabelOrElse)
{
    const auto diagram = parsed(QStringLiteral("if (ok?) then\n:x;\nendif\n"));

    const auto& branches = nodeAt<ActivityIf>(diagram.body, 0).branches;
    ASSERT_EQ(branches.size(), 1u);
    EXPECT_TRUE(branches[0].guard.isEmpty());
}

TEST(ActivityParserTest, ReadsWhileAndRepeat)
{
    const auto diagram = parsed(QStringLiteral("while (more?) is (yes)\n"
                                               ":work;\n"
                                               "endwhile (done)\n"
                                               "repeat :first;\n"
                                               ":again;\n"
                                               "repeat while (retry?) is (again) not (stop)\n"
                                               "repeat\n"
                                               ":plain;\n"
                                               "repeat while (x?)\n"));

    const auto& loop = nodeAt<ActivityWhile>(diagram.body, 0);
    EXPECT_EQ(loop.condition, QStringLiteral("more?"));
    EXPECT_EQ(loop.yesLabel, QStringLiteral("yes"));
    EXPECT_EQ(loop.exitLabel, QStringLiteral("done"));
    ASSERT_EQ(loop.block.size(), 1u);

    const auto& withFirst = nodeAt<ActivityRepeat>(diagram.body, 1);
    ASSERT_EQ(withFirst.block.size(), 2u);
    EXPECT_EQ(nodeAt<ActivityAction>(withFirst.block, 0).text, QStringLiteral("first"));
    EXPECT_EQ(withFirst.condition, QStringLiteral("retry?"));
    EXPECT_EQ(withFirst.yesLabel, QStringLiteral("again"));
    EXPECT_EQ(withFirst.exitLabel, QStringLiteral("stop"));

    const auto& plain = nodeAt<ActivityRepeat>(diagram.body, 2);
    EXPECT_EQ(plain.block.size(), 1u);
    EXPECT_TRUE(plain.yesLabel.isEmpty());
    EXPECT_TRUE(plain.exitLabel.isEmpty());
}

TEST(ActivityParserTest, ReadsForkWithAnyNumberOfBranches)
{
    const auto diagram = parsed(QStringLiteral("fork\n:a;\nfork again\n:b;\nfork again\n:c;\n"
                                               "end fork\nfork\n:d;\nfork again\n:e;\n"
                                               "end merge\n"));

    EXPECT_EQ(nodeAt<ActivityFork>(diagram.body, 0).branches.size(), 3u);
    EXPECT_EQ(nodeAt<ActivityFork>(diagram.body, 1).branches.size(), 2u);
}

TEST(ActivityParserTest, ReadsNestedAndQuotedPartitions)
{
    const auto diagram = parsed(QStringLiteral("partition Outer {\n"
                                               "partition \"Inner part\" {\n"
                                               ":x;\n"
                                               "}\n"
                                               "}\n"));

    const auto& outer = nodeAt<ActivityPartition>(diagram.body, 0);
    EXPECT_EQ(outer.label, QStringLiteral("Outer"));
    const auto& inner = nodeAt<ActivityPartition>(outer.block, 0);
    EXPECT_EQ(inner.label, QStringLiteral("Inner part"));
    EXPECT_EQ(inner.block.size(), 1u);
}

TEST(ActivityParserTest, AttachesNotesToThePrecedingAction)
{
    const auto diagram = parsed(QStringLiteral("start\n"
                                               ":a;\n"
                                               "note left : short note\n"
                                               ":b;\n"
                                               "note right\n"
                                               "first line\n"
                                               "second line\n"
                                               "end note\n"
                                               ":c;\n"));

    const auto& first = nodeAt<ActivityAction>(diagram.body, 1);
    ASSERT_TRUE(first.note.has_value());
    EXPECT_EQ(first.note->side, ActivityNoteSide::Left);
    EXPECT_EQ(first.note->text, QStringLiteral("short note"));
    const auto& second = nodeAt<ActivityAction>(diagram.body, 2);
    ASSERT_TRUE(second.note.has_value());
    EXPECT_EQ(second.note->side, ActivityNoteSide::Right);
    EXPECT_EQ(second.note->text, QStringLiteral("first line\nsecond line"));
    EXPECT_FALSE(nodeAt<ActivityAction>(diagram.body, 3).note.has_value());
}

TEST(ActivityParserTest, ReadsDetachAndKill)
{
    const auto diagram = parsed(QStringLiteral("if (x) then (y)\n:a;\ndetach\nelse (n)\n:b;\nkill\n"
                                               "endif\n"));

    const auto& branches = nodeAt<ActivityIf>(diagram.body, 0).branches;
    EXPECT_EQ(nodeAt<ActivityBranchEnd>(branches[0].block, 1).kind, BranchEndKind::Detach);
    EXPECT_EQ(nodeAt<ActivityBranchEnd>(branches[1].block, 1).kind, BranchEndKind::Kill);
}

TEST(ActivityParserTest, IgnoresDecorationsWithWarnings)
{
    const auto diagram = parsed(QStringLiteral("' a comment\n"
                                               "/' block\ncomment '/\n"
                                               "skinparam monochrome true\n"
                                               "skinparam activity {\nBackgroundColor red\n}\n"
                                               "!pragma teoz true\n"
                                               "hide footbox\n"
                                               "scale 2\n"
                                               "caption Figure\n"
                                               "header top\n"
                                               "footer bottom\n"
                                               "legend\nentry\nendlegend\n"
                                               "<style>\nactivity { }\n</style>\n"
                                               "start\n"));

    EXPECT_EQ(diagram.body.size(), 1u);
    EXPECT_EQ(diagram.warnings,
              (QStringList { QStringLiteral("Line 4: \"skinparam\" ignored"),
                             QStringLiteral("Line 5: \"skinparam\" block ignored"),
                             QStringLiteral("Line 8: \"!pragma teoz true\" ignored"),
                             QStringLiteral("Line 9: \"hide\" ignored"),
                             QStringLiteral("Line 10: \"scale\" ignored"),
                             QStringLiteral("Line 11: \"caption\" ignored"),
                             QStringLiteral("Line 12: \"header\" ignored"),
                             QStringLiteral("Line 13: \"footer\" ignored"),
                             QStringLiteral("Line 14: \"legend\" block ignored"),
                             QStringLiteral("Line 17: \"style\" block ignored") }));
}

TEST(ActivityParserTest, ReportsValidButUnsupportedPlantUml)
{
    struct Case
    {
        QString text;
        QString line;
        QString feature;
        QString rewrite;
    };
    const QList<Case> cases {
        { QStringLiteral("|Customer|"), QStringLiteral("|Customer|"), QStringLiteral("swimlanes"),
          QStringLiteral("Show who does what with a sequence diagram, or drop the lanes and name "
                         "the actor in each action") },
        { QStringLiteral("switch (kind?)"), QStringLiteral("switch (kind?)"),
          QStringLiteral("switch"), QStringLiteral("Write it as if / elseif / else / endif") },
        { QStringLiteral("(*) --> \"First\""), QStringLiteral("(*) --> \"First\""),
          QStringLiteral("the old activity syntax"),
          QStringLiteral("Use the new syntax: start, :action;, if (...) then, stop") },
        { QStringLiteral("goto somewhere"), QStringLiteral("goto somewhere"),
          QStringLiteral("jumps and connectors"),
          QStringLiteral("Restructure the flow with while or repeat") },
        { QStringLiteral("(A)"), QStringLiteral("(A)"), QStringLiteral("jumps and connectors"),
          QStringLiteral("Restructure the flow with while or repeat") },
        { QStringLiteral("split"), QStringLiteral("split"), QStringLiteral("split"),
          QStringLiteral("Use fork / fork again / end fork") },
        { QStringLiteral("break"), QStringLiteral("break"), QStringLiteral("break"),
          QStringLiteral("End the branch with detach, or move the exit into the loop "
                         "condition") },
        { QStringLiteral("backward :undo;"), QStringLiteral("backward :undo;"),
          QStringLiteral("backward actions"),
          QStringLiteral("Move the action into the repeat body") },
        { QStringLiteral("if (kind) is (a) then"), QStringLiteral("if (kind) is (a) then"),
          QStringLiteral("is/equals conditions"),
          QStringLiteral("Write the whole condition in the parentheses: if (a = b?) then "
                         "(yes)") },
        { QStringLiteral("floating note left : hi"), QStringLiteral("floating note left : hi"),
          QStringLiteral("floating notes"),
          QStringLiteral("Attach the note to an action: put it right after the action") },
    };

    for (const Case& entry : cases)
    {
        EXPECT_EQ(parseError(QStringLiteral("start\n") + entry.text + QLatin1Char('\n')),
                  QStringLiteral("Line 2: ")
                      + unsupportedText(entry.line, entry.feature, entry.rewrite))
            << entry.text.toStdString();
    }
}

TEST(ActivityParserTest, ASwimlaneIsReportedEvenBeforeTheFirstAction)
{
    EXPECT_TRUE(parseError(QStringLiteral("|Customer|\nstart\n"))
                    .startsWith(QStringLiteral("Line 1: \"|Customer|\" is valid PlantUML, but "
                                               "this app does not support swimlanes.")));
}

TEST(ActivityParserTest, ReportsANoteThatDoesNotFollowAnAction)
{
    const QString expected
        = QStringLiteral("Line 2: a note must follow an action; real PlantUML would silently drop "
                         "this one. Move it right after the action it describes.");

    EXPECT_EQ(parseError(QStringLiteral("start\nnote right : lost\n")), expected);
    EXPECT_EQ(parseError(QStringLiteral(":a;\nstart\nnote right : lost\n")),
              QStringLiteral("Line 3: ") + expected.mid(QStringLiteral("Line 2: ").size()));
    EXPECT_EQ(parseError(QStringLiteral(":a;\nif (x) then (y)\nnote left : lost\nendif\n")),
              QStringLiteral("Line 3: ") + expected.mid(QStringLiteral("Line 2: ").size()));
}

TEST(ActivityParserTest, ReportsASecondNoteOnOneAction)
{
    EXPECT_EQ(parseError(QStringLiteral(":a;\nnote left : one\nnote right : two\n")),
              QStringLiteral("Line 3: an action can carry only one note. Merge the two notes into "
                             "one."));
}

TEST(ActivityParserTest, ReportsAnUnknownStatementWithWhatIsUnderstood)
{
    EXPECT_EQ(parseError(QStringLiteral("start\nwhatever happens\n")),
              QStringLiteral("Line 2: unknown statement \"whatever happens\". An activity diagram "
                             "here understands: start, stop, end, :action;, -> label;, "
                             "if/elseif/else/endif, while/endwhile, repeat/repeat while, "
                             "fork/fork again/end fork, partition Name { }, note left|right, "
                             "detach, kill."));
}

TEST(ActivityParserTest, ReportsAMalformedKnownStatementWithItsForm)
{
    EXPECT_EQ(parseError(QStringLiteral("start\nif x then\n")),
              QStringLiteral("Line 2: \"if x then\" is not a valid \"if\" statement. Write it as: "
                             "if (condition) then (yes)."));
    EXPECT_EQ(parseError(QStringLiteral("start\nwhile\n")),
              QStringLiteral("Line 2: \"while\" is not a valid \"while\" statement. Write it as: "
                             "while (condition) is (yes)."));
    EXPECT_EQ(parseError(QStringLiteral("start\npartition Name\n")),
              QStringLiteral("Line 2: \"partition Name\" is not a valid \"partition\" statement. "
                             "Write it as: partition Name {."));
}

TEST(ActivityParserTest, ReportsAnActionWithoutItsColon)
{
    EXPECT_EQ(parseError(QStringLiteral("start\nDo something;\n")),
              QStringLiteral("Line 2: \"Do something;\" looks like an action without its leading "
                             "colon. Write it as \":Do something;\"."));
}

TEST(ActivityParserTest, ReportsClosersWithoutAnOpener)
{
    EXPECT_EQ(parseError(QStringLiteral("start\nendif\n")),
              QStringLiteral("Line 2: \"endif\" without an open \"if\""));
    EXPECT_EQ(parseError(QStringLiteral("start\nelse (no)\n")),
              QStringLiteral("Line 2: \"else\" without an open \"if\""));
    EXPECT_EQ(parseError(QStringLiteral("start\nendwhile\n")),
              QStringLiteral("Line 2: \"endwhile\" without an open \"while\""));
    EXPECT_EQ(parseError(QStringLiteral("start\nrepeat while (x?)\n")),
              QStringLiteral("Line 2: \"repeat while\" without an open \"repeat\""));
    EXPECT_EQ(parseError(QStringLiteral("start\nfork again\n")),
              QStringLiteral("Line 2: \"fork again\" without an open \"fork\""));
    EXPECT_EQ(parseError(QStringLiteral("start\nend fork\n")),
              QStringLiteral("Line 2: \"end fork\" without an open \"fork\""));
    EXPECT_EQ(parseError(QStringLiteral("start\n}\n")),
              QStringLiteral("Line 2: \"}\" without an open \"partition\""));
}

TEST(ActivityParserTest, ReportsACloserThatClosesTheWrongBlock)
{
    EXPECT_EQ(parseError(QStringLiteral("while (a?) is (y)\nif (b?) then (y)\n:x;\nendwhile\n")),
              QStringLiteral("Line 4: \"endwhile\" does not match the \"if\" opened at line 2. "
                             "Close it first with \"endif\"."));
}

TEST(ActivityParserTest, ReportsAnElseAfterAnElse)
{
    EXPECT_EQ(parseError(QStringLiteral(":a;\nif (x) then (y)\n:b;\nelse (n)\n:c;\nelse (z)\n"
                                        "endif\n")),
              QStringLiteral("Line 6: \"else\" after \"else\" in the \"if\" opened at line 2. An "
                             "\"if\" has one \"else\", and it comes last; use \"elseif\" for more "
                             "conditions."));
    EXPECT_TRUE(parseError(QStringLiteral("if (x) then (y)\nelse (n)\nelseif (z) then\nendif\n"))
                    .startsWith(QStringLiteral("Line 3: \"elseif\" after \"else\"")));
}

TEST(ActivityParserTest, ReportsAnUnclosedBlockAtTheLastLine)
{
    EXPECT_EQ(parseError(QStringLiteral("start\nwhile (a?) is (y)\n:x;\n")),
              QStringLiteral("Line 3: the \"while\" opened at line 2 is not closed with "
                             "\"endwhile\". Add \"endwhile\" where the block ends."));
    EXPECT_EQ(parseError(QStringLiteral("if (a?) then (y)\n:x;\n")),
              QStringLiteral("Line 2: the \"if\" opened at line 1 is not closed with \"endif\". "
                             "Add \"endif\" where the block ends."));
    EXPECT_EQ(parseError(QStringLiteral("partition P {\n:x;\n\n")),
              QStringLiteral("Line 2: the \"partition\" opened at line 1 is not closed with "
                             "\"}\". Add \"}\" where the block ends."));
}

TEST(ActivityParserTest, ReportsAnActionWithoutItsSemicolon)
{
    EXPECT_EQ(parseError(QStringLiteral("start\n:first\nsecond\n")),
              QStringLiteral("Line 3: the action started at line 2 has no closing \";\". End the "
                             "action text with \";\"."));
}

TEST(ActivityParserTest, ReportsANoteWithoutItsEnd)
{
    EXPECT_EQ(parseError(QStringLiteral(":a;\nnote left\ntext\n")),
              QStringLiteral("Line 3: the note started at line 2 has no closing \"end note\". Add "
                             "\"end note\" after the text."));
}

TEST(ActivityParserTest, ReportsAnEmptyDiagramWithoutALine)
{
    EXPECT_EQ(parseError(QStringLiteral("@startuml\n@enduml\n")),
              QStringLiteral("The diagram has no elements"));
    EXPECT_EQ(parseError(QStringLiteral("title Only a title\n")),
              QStringLiteral("The diagram has no elements"));
}

TEST(ActivityParserTest, LimitsTheLengthAndTheNumberOfElements)
{
    QString many;
    for (int index = 0; index < 301; ++index)
        many += QStringLiteral(":a;\n");
    EXPECT_EQ(parseError(many), QStringLiteral("Line 301: the diagram has more than 300 elements"));

    QString exact;
    for (int index = 0; index < 300; ++index)
        exact += QStringLiteral(":a;\n");
    EXPECT_EQ(parsed(exact).body.size(), 300u);

    EXPECT_EQ(parseError(QStringLiteral(":") + QString(20001, QLatin1Char('x')) + QLatin1Char(';')),
              QStringLiteral("The diagram is longer than 20000 characters"));
}

TEST(ActivityParserTest, StopsAtTheFirstError)
{
    EXPECT_TRUE(parseError(QStringLiteral("start\nendif\nwhatever\n"))
                    .startsWith(QStringLiteral("Line 2:")));
}
