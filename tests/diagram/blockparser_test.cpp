#include <gtest/gtest.h>

#include "diagram/blockparser.h"

using namespace diagram;

namespace
{
const QString kSocDiagram = QStringLiteral("@startuml\n"
                                           "rectangle \"SoC\" {\n"
                                           "  rectangle \"CPU\" as CPU\n"
                                           "  rectangle \"Boot ROM\" as ROM\n"
                                           "  rectangle \"SRAM\\nsmall, ready at once\" as SRAM\n"
                                           "  rectangle \"DDR controller\" as CTRL\n"
                                           "}\n"
                                           "rectangle \"external DDR\" as DDR\n"
                                           "CPU -- ROM\n"
                                           "CPU -- SRAM\n"
                                           "CPU -- CTRL\n"
                                           "CTRL -- DDR\n"
                                           "@enduml\n");

BlockDiagram parsed(const QString& text)
{
    const auto result = BlockParser::parse(text);
    EXPECT_TRUE(result.isSuccess()) << (result.isFailure() ? result.error().toStdString() : "");
    return result.isSuccess() ? result.value() : BlockDiagram {};
}

QString parseError(const QString& text)
{
    const auto result = BlockParser::parse(text);
    EXPECT_TRUE(result.isFailure());
    return result.isFailure() ? result.error() : QString();
}

QString unsupportedText(const QString& line, const QString& feature, const QString& rewrite)
{
    return QStringLiteral("\"%1\" is valid PlantUML, but this app does not support %2. %3.")
        .arg(line, feature, rewrite);
}

int indexOfBlock(const BlockDiagram& diagram, const QString& id)
{
    for (qsizetype index = 0; index < diagram.blocks.size(); ++index)
    {
        if (diagram.blocks.at(index).id == id)
            return int(index);
    }
    return -1;
}
}

TEST(BlockParserTest, ReadsBlocksNestingLabelsAndLinks)
{
    const auto diagram = parsed(kSocDiagram);

    ASSERT_EQ(diagram.blocks.size(), 6);
    EXPECT_EQ(diagram.blocks.at(0).id, QStringLiteral("SoC"));
    EXPECT_TRUE(diagram.blocks.at(0).isContainer);
    EXPECT_EQ(diagram.blocks.at(0).parent, -1);
    EXPECT_EQ(diagram.blocks.at(1).id, QStringLiteral("CPU"));
    EXPECT_EQ(diagram.blocks.at(1).parent, 0);
    EXPECT_FALSE(diagram.blocks.at(1).isContainer);
    EXPECT_EQ(diagram.blocks.at(2).label, QStringLiteral("Boot ROM"));
    EXPECT_EQ(diagram.blocks.at(3).label, QStringLiteral("SRAM\nsmall, ready at once"));
    EXPECT_EQ(diagram.blocks.at(5).id, QStringLiteral("DDR"));
    EXPECT_EQ(diagram.blocks.at(5).parent, -1);
    EXPECT_EQ(diagram.blocks.at(5).line, 8);

    ASSERT_EQ(diagram.links.size(), 4);
    EXPECT_EQ(diagram.links.at(0).from, indexOfBlock(diagram, QStringLiteral("CPU")));
    EXPECT_EQ(diagram.links.at(0).to, indexOfBlock(diagram, QStringLiteral("ROM")));
    EXPECT_EQ(diagram.links.at(3).from, indexOfBlock(diagram, QStringLiteral("CTRL")));
    EXPECT_EQ(diagram.links.at(3).to, indexOfBlock(diagram, QStringLiteral("DDR")));
    EXPECT_EQ(diagram.links.at(3).sourceLine, 12);
    EXPECT_TRUE(diagram.warnings.isEmpty());
}

TEST(BlockParserTest, ReadsEveryKindOfBlock)
{
    const auto diagram = parsed(QStringLiteral("rectangle \"A\" as a\n"
                                               "node \"B\" as b\n"
                                               "component \"C\" as c\n"
                                               "database \"D\" as d\n"));

    ASSERT_EQ(diagram.blocks.size(), 4);
    EXPECT_EQ(diagram.blocks.at(0).kind, BlockKind::Rectangle);
    EXPECT_EQ(diagram.blocks.at(1).kind, BlockKind::Node);
    EXPECT_EQ(diagram.blocks.at(2).kind, BlockKind::Component);
    EXPECT_EQ(diagram.blocks.at(3).kind, BlockKind::Database);
}

TEST(BlockParserTest, ReadsBlocksWithoutAnAliasAndWithAQuotedAlias)
{
    const auto diagram = parsed(QStringLiteral("rectangle CPU\n"
                                               "rectangle \"Boot ROM\"\n"
                                               "rectangle ROM2 as \"Second ROM\"\n"
                                               "\"Boot ROM\" -- CPU\n"));

    ASSERT_EQ(diagram.blocks.size(), 3);
    EXPECT_EQ(diagram.blocks.at(0).id, QStringLiteral("CPU"));
    EXPECT_EQ(diagram.blocks.at(0).label, QStringLiteral("CPU"));
    EXPECT_EQ(diagram.blocks.at(1).id, QStringLiteral("Boot ROM"));
    EXPECT_EQ(diagram.blocks.at(2).id, QStringLiteral("ROM2"));
    EXPECT_EQ(diagram.blocks.at(2).label, QStringLiteral("Second ROM"));
    ASSERT_EQ(diagram.links.size(), 1);
    EXPECT_EQ(diagram.links.at(0).from, 1);
    EXPECT_EQ(diagram.links.at(0).to, 0);
}

TEST(BlockParserTest, NestsContainersTwoLevelsDeep)
{
    const auto diagram = parsed(QStringLiteral("rectangle \"Board\" {\n"
                                               "  rectangle \"SoC\" {\n"
                                               "    rectangle CPU\n"
                                               "  }\n"
                                               "  database FLASH\n"
                                               "}\n"
                                               "rectangle Outside\n"));

    ASSERT_EQ(diagram.blocks.size(), 5);
    EXPECT_EQ(diagram.blocks.at(1).parent, 0);
    EXPECT_EQ(diagram.blocks.at(2).parent, 1);
    EXPECT_EQ(diagram.blocks.at(3).parent, 0);
    EXPECT_EQ(diagram.blocks.at(4).parent, -1);
}

TEST(BlockParserTest, ReadsAContainerWithAnAliasAndAnEmptyContainer)
{
    const auto diagram = parsed(QStringLiteral("node \"Server\" as S {\n}\nrectangle Box {\n}\n"));

    ASSERT_EQ(diagram.blocks.size(), 2);
    EXPECT_EQ(diagram.blocks.at(0).id, QStringLiteral("S"));
    EXPECT_EQ(diagram.blocks.at(0).kind, BlockKind::Node);
    EXPECT_TRUE(diagram.blocks.at(0).isContainer);
    EXPECT_TRUE(diagram.blocks.at(1).isContainer);
}

TEST(BlockParserTest, ReadsEveryFormOfLinkWithItsLabel)
{
    const auto diagram = parsed(QStringLiteral("rectangle A\nrectangle B\n"
                                               "A -- B\n"
                                               "A --> B : reads\n"
                                               "A <-- B : replies\n"
                                               "A .. B : maybe\n"));

    ASSERT_EQ(diagram.links.size(), 4);
    EXPECT_EQ(diagram.links.at(0).line, BlockLinkLine::Solid);
    EXPECT_EQ(diagram.links.at(0).head, BlockLinkHead::None);
    EXPECT_TRUE(diagram.links.at(0).label.isEmpty());
    EXPECT_EQ(diagram.links.at(1).head, BlockLinkHead::ToTarget);
    EXPECT_EQ(diagram.links.at(1).label, QStringLiteral("reads"));
    EXPECT_EQ(diagram.links.at(2).from, 0);
    EXPECT_EQ(diagram.links.at(2).to, 1);
    EXPECT_EQ(diagram.links.at(2).head, BlockLinkHead::ToSource);
    EXPECT_EQ(diagram.links.at(2).label, QStringLiteral("replies"));
    EXPECT_EQ(diagram.links.at(3).line, BlockLinkLine::Dotted);
    EXPECT_EQ(diagram.links.at(3).head, BlockLinkHead::None);
    EXPECT_EQ(diagram.links.at(3).label, QStringLiteral("maybe"));
}

TEST(BlockParserTest, ReadsTitlesAndNotesOnBothSides)
{
    const auto diagram = parsed(QStringLiteral("title Memory map\n"
                                               "rectangle CPU\n"
                                               "rectangle ROM\n"
                                               "note left of CPU : the core\n"
                                               "note right of ROM : read only\n"));

    EXPECT_EQ(diagram.title, QStringLiteral("Memory map"));
    ASSERT_EQ(diagram.notes.size(), 2);
    EXPECT_EQ(diagram.notes.at(0).block, 0);
    EXPECT_EQ(diagram.notes.at(0).side, BlockNoteSide::Left);
    EXPECT_EQ(diagram.notes.at(0).text, QStringLiteral("the core"));
    EXPECT_EQ(diagram.notes.at(1).block, 1);
    EXPECT_EQ(diagram.notes.at(1).side, BlockNoteSide::Right);
}

TEST(BlockParserTest, ReadsAMultiLineNote)
{
    const auto diagram = parsed(QStringLiteral("rectangle CPU\n"
                                               "note right of CPU\n"
                                               "first line\n"
                                               "second line\n"
                                               "end note\n"));

    ASSERT_EQ(diagram.notes.size(), 1);
    EXPECT_EQ(diagram.notes.at(0).text, QStringLiteral("first line\nsecond line"));
}

TEST(BlockParserTest, IgnoresStyleDirectivesWithAWarning)
{
    const auto diagram = parsed(QStringLiteral("skinparam monochrome true\n"
                                               "rectangle A <<soc>> #red\n"
                                               "' comment\n"));

    ASSERT_EQ(diagram.blocks.size(), 1);
    EXPECT_EQ(diagram.warnings,
              (QStringList { QStringLiteral("Line 1: \"skinparam\" ignored"),
                             QStringLiteral("Line 2: stereotype ignored"),
                             QStringLiteral("Line 2: colour ignored") }));
}

TEST(BlockParserTest, ReportsALinkToAnUnknownBlockAtItsLine)
{
    EXPECT_EQ(parseError(QStringLiteral("rectangle A\nA -- B\n")),
              QStringLiteral("Line 2: unknown block \"B\" in the link. Declare it before the "
                             "link, for example: rectangle \"B\" as B."));
}

TEST(BlockParserTest, ReportsAContainerThatIsNotClosedAtTheLastLine)
{
    EXPECT_EQ(parseError(QStringLiteral("rectangle \"SoC\" as SOC {\n"
                                        "  rectangle CPU\n"
                                        "@enduml\n")),
              QStringLiteral("Line 3: the container \"SOC\" opened at line 1 is not closed with "
                             "\"}\". Add \"}\" where the container ends."));
}

TEST(BlockParserTest, ReportsACloserWithoutAContainer)
{
    EXPECT_EQ(parseError(QStringLiteral("rectangle A\n}\n")),
              QStringLiteral("Line 2: \"}\" without an open container"));
}

TEST(BlockParserTest, ReportsADuplicateAliasAndALinkToItself)
{
    EXPECT_EQ(parseError(QStringLiteral("rectangle A\nnode A\n")),
              QStringLiteral("Line 2: the block \"A\" is already declared at line 1. Give each "
                             "block its own alias."));
    EXPECT_TRUE(parseError(QStringLiteral("rectangle A\nA -- A\n"))
                    .startsWith(QStringLiteral("Line 2: the link \"A -- A\" joins a block to "
                                               "itself")));
}

TEST(BlockParserTest, ReportsNotesThatCannotBeAttached)
{
    EXPECT_EQ(parseError(QStringLiteral("rectangle A\nnote left of B : x\n")),
              QStringLiteral("Line 2: unknown block \"B\" in the note. Declare it before the "
                             "note."));
    EXPECT_EQ(parseError(QStringLiteral("rectangle A\nnote left of A : x\nnote left of A : y\n")),
              QStringLiteral("Line 3: the block \"A\" already has a note on the left. Merge the "
                             "two notes into one."));
    EXPECT_EQ(parseError(QStringLiteral("rectangle A\nnote right of A\ntext\n")),
              QStringLiteral("Line 3: the note started at line 2 has no closing \"end note\". "
                             "Add \"end note\" after the text."));
}

TEST(BlockParserTest, ReportsAnEmptyDiagramWithoutALine)
{
    EXPECT_EQ(parseError(QStringLiteral("@startuml\ntitle Nothing\n@enduml\n")),
              QStringLiteral("The diagram has no blocks"));
}

TEST(BlockParserTest, RejectsValidPlantUmlThatIsNotDrawn)
{
    const QString rewriteBlock = QStringLiteral("Draw it as a rectangle, node, component or "
                                                "database, and group blocks with a rectangle and "
                                                "{ }");

    EXPECT_EQ(parseError(QStringLiteral("rectangle A\ncloud \"Internet\" as NET\n")),
              QStringLiteral("Line 2: ")
                  + unsupportedText(QStringLiteral("cloud \"Internet\" as NET"),
                                    QStringLiteral("\"cloud\" elements"), rewriteBlock));
    EXPECT_EQ(parseError(QStringLiteral("rectangle A\npackage Drivers {\n")),
              QStringLiteral("Line 2: ")
                  + unsupportedText(QStringLiteral("package Drivers {"),
                                    QStringLiteral("\"package\" elements"), rewriteBlock));
    EXPECT_EQ(parseError(QStringLiteral("rectangle A\ninterface Api\n")),
              QStringLiteral("Line 2: ")
                  + unsupportedText(QStringLiteral("interface Api"), QStringLiteral("interfaces"),
                                    QStringLiteral("Draw the interface as a small rectangle and "
                                                   "link to it")));
    EXPECT_EQ(parseError(QStringLiteral("rectangle A\n[Component] as C\n")),
              QStringLiteral("Line 2: ")
                  + unsupportedText(QStringLiteral("[Component] as C"),
                                    QStringLiteral("the [Component] short syntax"),
                                    QStringLiteral("Declare the block as: component \"Name\" as "
                                                   "Alias")));
}

TEST(BlockParserTest, RejectsArrowsThatAreNotDrawn)
{
    const QString expected = QStringLiteral("Line 3: ")
        + unsupportedText(QStringLiteral("A -> B"), QStringLiteral("this kind of arrow"),
                          QStringLiteral("Write the link as A -- B, A --> B, A <-- B or A .. B"));

    EXPECT_EQ(parseError(QStringLiteral("rectangle A\nrectangle B\nA -> B\n")), expected);
    EXPECT_TRUE(parseError(QStringLiteral("rectangle A\nrectangle B\nA ..> B\n"))
                    .contains(QStringLiteral("does not support this kind of arrow")));
    EXPECT_TRUE(parseError(QStringLiteral("rectangle A\nrectangle B\nA -down-> B\n"))
                    .contains(QStringLiteral("does not support this kind of arrow")));
}

TEST(BlockParserTest, RejectsNotesAndDirectionsThatAreNotDrawn)
{
    EXPECT_TRUE(parseError(QStringLiteral("rectangle A\nnote top of A : x\n"))
                    .contains(QStringLiteral("does not support notes above or below a block")));
    EXPECT_TRUE(parseError(QStringLiteral("rectangle A\nfloating note : x\n"))
                    .contains(QStringLiteral("does not support floating notes")));
    EXPECT_TRUE(parseError(QStringLiteral("left to right direction\nrectangle A\n"))
                    .contains(QStringLiteral("does not support layout directions")));
}

TEST(BlockParserTest, NamesTheFormOfAKnownKeywordWrittenWrongly)
{
    EXPECT_EQ(parseError(QStringLiteral("rectangle foo-bar\n")),
              QStringLiteral("Line 1: \"rectangle foo-bar\" is not a valid \"block\" statement. "
                             "Write it as: rectangle \"Label\" as Alias, or rectangle \"Label\" { "
                             "to open a container."));
    EXPECT_EQ(parseError(QStringLiteral("rectangle A\nnote left : x\n")),
              QStringLiteral("Line 2: \"note left : x\" is not a valid \"note\" statement. Write "
                             "it as: note left of Alias : text or note right of Alias : text."));
}

TEST(BlockParserTest, ListsWhatItUnderstandsForAnUnknownStatement)
{
    const QString error = parseError(QStringLiteral("rectangle A\nwhatever\n"));

    EXPECT_TRUE(error.startsWith(QStringLiteral("Line 2: unknown statement \"whatever\". A block "
                                                "diagram here understands: ")));
}
