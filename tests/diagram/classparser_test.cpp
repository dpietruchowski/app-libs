#include <gtest/gtest.h>

#include "diagram/classparser.h"

using namespace diagram;

namespace
{
const QString kKernel = QStringLiteral("@startuml\n"
                                       "title Process memory\n"
                                       "struct task_struct {\n"
                                       "  pid : int\n"
                                       "  mm : mm_struct*\n"
                                       "}\n"
                                       "struct mm_struct {\n"
                                       "  mmap : vm_area_struct*\n"
                                       "}\n"
                                       "struct vm_area_struct {\n"
                                       "  vm_start : unsigned long\n"
                                       "  --\n"
                                       "  vm_next : vm_area_struct*\n"
                                       "}\n"
                                       "struct files_struct\n"
                                       "task_struct \"1\" --> \"0..1\" mm_struct : mm\n"
                                       "task_struct --> files_struct : files\n"
                                       "mm_struct \"1\" *-- \"many\" vm_area_struct : mmap\n"
                                       "vm_area_struct --> vm_area_struct : vm_next\n"
                                       "@enduml\n");

ClassDiagram parsed(const QString& text)
{
    const auto result = ClassParser::parse(text);
    EXPECT_TRUE(result.isSuccess()) << (result.isFailure() ? result.error().toStdString() : "");
    return result.isSuccess() ? result.value() : ClassDiagram {};
}

QString parseError(const QString& text)
{
    const auto result = ClassParser::parse(text);
    EXPECT_TRUE(result.isFailure());
    return result.isFailure() ? result.error() : QString();
}

int indexOfType(const ClassDiagram& diagram, const QString& id)
{
    for (qsizetype index = 0; index < diagram.types.size(); ++index)
    {
        if (diagram.types.at(index).id == id)
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
    EXPECT_TRUE(error.contains(QStringLiteral("A class diagram here understands: ")))
        << error.toStdString();
}
}

TEST(ClassParserTest, ReadsStructsMembersCompartmentsAndRelations)
{
    const auto diagram = parsed(kKernel);

    EXPECT_EQ(diagram.title, QStringLiteral("Process memory"));
    ASSERT_EQ(diagram.types.size(), 4);
    EXPECT_EQ(diagram.types.at(0).id, QStringLiteral("task_struct"));
    EXPECT_EQ(diagram.types.at(0).kind, ClassKind::Struct);
    ASSERT_EQ(diagram.types.at(0).compartments.size(), 1);
    EXPECT_EQ(diagram.types.at(0).compartments.at(0),
              (QStringList { QStringLiteral("pid : int"), QStringLiteral("mm : mm_struct*") }));
    ASSERT_EQ(diagram.types.at(2).compartments.size(), 2);
    EXPECT_EQ(diagram.types.at(2).compartments.at(1),
              QStringList { QStringLiteral("vm_next : vm_area_struct*") });
    EXPECT_TRUE(diagram.types.at(3).compartments.isEmpty());

    ASSERT_EQ(diagram.relations.size(), 4);
    EXPECT_EQ(diagram.relations.at(0).from, 0);
    EXPECT_EQ(diagram.relations.at(0).to, 1);
    EXPECT_EQ(diagram.relations.at(0).label, QStringLiteral("mm"));
    EXPECT_EQ(diagram.relations.at(0).sourceLine, 16);
    EXPECT_EQ(diagram.relations.at(3).from, diagram.relations.at(3).to);
    EXPECT_TRUE(diagram.warnings.isEmpty());
}

TEST(ClassParserTest, ReadsAllFiveKindsAndAQuotedNameWithAnAlias)
{
    const auto diagram = parsed(QStringLiteral("class Plain\n"
                                               "abstract class Base\n"
                                               "struct Record\n"
                                               "enum Color\n"
                                               "interface Shape\n"
                                               "class \"Long name\" as Alias\n"
                                               "class Other as \"Second\\nname\"\n"));

    ASSERT_EQ(diagram.types.size(), 7);
    EXPECT_EQ(diagram.types.at(0).kind, ClassKind::Class);
    EXPECT_EQ(diagram.types.at(1).kind, ClassKind::Abstract);
    EXPECT_EQ(diagram.types.at(2).kind, ClassKind::Struct);
    EXPECT_EQ(diagram.types.at(3).kind, ClassKind::Enum);
    EXPECT_EQ(diagram.types.at(4).kind, ClassKind::Interface);
    EXPECT_EQ(diagram.types.at(5).id, QStringLiteral("Alias"));
    EXPECT_EQ(diagram.types.at(5).label, QStringLiteral("Long name"));
    EXPECT_EQ(diagram.types.at(6).id, QStringLiteral("Other"));
    EXPECT_EQ(diagram.types.at(6).label, QStringLiteral("Second\nname"));
}

TEST(ClassParserTest, KeepsGenericNamesAsTextAndReachesThemById)
{
    const auto diagram = parsed(QStringLiteral("class List<T>\nList --> Item\n"));

    ASSERT_EQ(diagram.types.size(), 2);
    EXPECT_EQ(diagram.types.at(0).id, QStringLiteral("List"));
    EXPECT_EQ(diagram.types.at(0).label, QStringLiteral("List<T>"));
    EXPECT_EQ(diagram.relations.at(0).from, 0);
}

TEST(ClassParserTest, CreatesAClassForAnUnknownRelationEndpoint)
{
    const auto diagram = parsed(QStringLiteral("class A\nA --> Missing\n"));

    ASSERT_EQ(diagram.types.size(), 2);
    EXPECT_EQ(diagram.types.at(1).id, QStringLiteral("Missing"));
    EXPECT_EQ(diagram.types.at(1).kind, ClassKind::Class);
}

TEST(ClassParserTest, LetsAnImplicitClassBeDeclaredLater)
{
    const auto diagram = parsed(QStringLiteral("A --> B\nstruct B {\n  x : int\n}\n"));

    ASSERT_EQ(diagram.types.size(), 2);
    EXPECT_EQ(diagram.types.at(1).kind, ClassKind::Struct);
    EXPECT_EQ(diagram.types.at(1).compartments.size(), 1);
}

TEST(ClassParserTest, ReadsInheritanceInBothDirections)
{
    const auto diagram = parsed(QStringLiteral("A <|-- B\nC --|> A\n"));

    ASSERT_EQ(diagram.relations.size(), 2);
    EXPECT_EQ(diagram.relations.at(0).fromEnd, ClassLinkEnd::Triangle);
    EXPECT_EQ(diagram.relations.at(0).toEnd, ClassLinkEnd::None);
    EXPECT_EQ(diagram.relations.at(0).line, ClassLinkLine::Solid);
    EXPECT_EQ(diagram.relations.at(1).fromEnd, ClassLinkEnd::None);
    EXPECT_EQ(diagram.relations.at(1).toEnd, ClassLinkEnd::Triangle);
}

TEST(ClassParserTest, ReadsRealizationAsADashedLineInBothDirections)
{
    const auto diagram = parsed(QStringLiteral("A <|.. B\nC ..|> A\n"));

    EXPECT_EQ(diagram.relations.at(0).fromEnd, ClassLinkEnd::Triangle);
    EXPECT_EQ(diagram.relations.at(0).line, ClassLinkLine::Dashed);
    EXPECT_EQ(diagram.relations.at(1).toEnd, ClassLinkEnd::Triangle);
    EXPECT_EQ(diagram.relations.at(1).line, ClassLinkLine::Dashed);
}

TEST(ClassParserTest, ReadsCompositionAggregationAssociationDependencyAndPlainLinks)
{
    const auto diagram = parsed(QStringLiteral("A *-- B\n"
                                               "A o-- C\n"
                                               "A --> D\n"
                                               "A ..> E\n"
                                               "A -- F\n"
                                               "A .. G\n"));

    ASSERT_EQ(diagram.relations.size(), 6);
    EXPECT_EQ(diagram.relations.at(0).fromEnd, ClassLinkEnd::FilledDiamond);
    EXPECT_EQ(diagram.relations.at(1).fromEnd, ClassLinkEnd::HollowDiamond);
    EXPECT_EQ(diagram.relations.at(2).toEnd, ClassLinkEnd::Arrow);
    EXPECT_EQ(diagram.relations.at(2).line, ClassLinkLine::Solid);
    EXPECT_EQ(diagram.relations.at(3).toEnd, ClassLinkEnd::Arrow);
    EXPECT_EQ(diagram.relations.at(3).line, ClassLinkLine::Dashed);
    EXPECT_EQ(diagram.relations.at(4).fromEnd, ClassLinkEnd::None);
    EXPECT_EQ(diagram.relations.at(4).toEnd, ClassLinkEnd::None);
    EXPECT_EQ(diagram.relations.at(4).line, ClassLinkLine::Solid);
    EXPECT_EQ(diagram.relations.at(5).line, ClassLinkLine::Dashed);
}

TEST(ClassParserTest, ReadsMultiplicitiesAtEitherEndAndALabel)
{
    const auto diagram = parsed(QStringLiteral("A \"1\" *-- \"0..*\" B : owns\n"
                                               "C \"2\" --> D\n"
                                               "E --> \"many\" F\n"));

    EXPECT_EQ(diagram.relations.at(0).fromMultiplicity, QStringLiteral("1"));
    EXPECT_EQ(diagram.relations.at(0).toMultiplicity, QStringLiteral("0..*"));
    EXPECT_EQ(diagram.relations.at(0).label, QStringLiteral("owns"));
    EXPECT_EQ(diagram.relations.at(1).fromMultiplicity, QStringLiteral("2"));
    EXPECT_TRUE(diagram.relations.at(1).toMultiplicity.isEmpty());
    EXPECT_TRUE(diagram.relations.at(2).fromMultiplicity.isEmpty());
    EXPECT_EQ(diagram.relations.at(2).toMultiplicity, QStringLiteral("many"));
}

TEST(ClassParserTest, KeepsVisibilityMarksAndBothMemberStylesAsWritten)
{
    const auto diagram = parsed(QStringLiteral("class Account {\n"
                                               "  - balance : Money\n"
                                               "  # Money limit\n"
                                               "  ~ owner\n"
                                               "  + deposit(amount : Money) : void\n"
                                               "}\n"));

    ASSERT_EQ(diagram.types.at(0).compartments.size(), 2);
    EXPECT_EQ(diagram.types.at(0).compartments.at(0),
              (QStringList { QStringLiteral("- balance : Money"), QStringLiteral("# Money limit"),
                             QStringLiteral("~ owner") }));
    EXPECT_EQ(diagram.types.at(0).compartments.at(1),
              QStringList { QStringLiteral("+ deposit(amount : Money) : void") });
}

TEST(ClassParserTest, KeepsAnEnumBodyAsOneCompartmentOfValues)
{
    const auto diagram = parsed(QStringLiteral("enum Color {\n  RED\n  GREEN\n  draw()\n}\n"));

    ASSERT_EQ(diagram.types.at(0).compartments.size(), 1);
    EXPECT_EQ(diagram.types.at(0).compartments.at(0).size(), 3);
}

TEST(ClassParserTest, ReadsABodyWrittenOnOneLine)
{
    const auto diagram = parsed(QStringLiteral("class A { }\nclass B {\n}\n"));

    ASSERT_EQ(diagram.types.size(), 2);
    EXPECT_TRUE(diagram.types.at(0).compartments.isEmpty());
    EXPECT_TRUE(diagram.types.at(1).compartments.isEmpty());
}

TEST(ClassParserTest, ReadsNotesOnBothSides)
{
    const auto diagram = parsed(QStringLiteral("class A\n"
                                               "class B\n"
                                               "note left of A : the root\n"
                                               "note right of B\n"
                                               "first\n"
                                               "second\n"
                                               "end note\n"));

    ASSERT_EQ(diagram.notes.size(), 2);
    EXPECT_EQ(diagram.notes.at(0).type, 0);
    EXPECT_EQ(diagram.notes.at(0).side, ClassNoteSide::Left);
    EXPECT_EQ(diagram.notes.at(0).text, QStringLiteral("the root"));
    EXPECT_EQ(diagram.notes.at(1).side, ClassNoteSide::Right);
    EXPECT_EQ(diagram.notes.at(1).text, QStringLiteral("first\nsecond"));
}

TEST(ClassParserTest, IgnoresScaleAndCaptionsWithAWarning)
{
    const auto diagram = parsed(QStringLiteral("scale 2\nclass A\ncaption Figure 1\n"));

    EXPECT_EQ(diagram.warnings,
              (QStringList { QStringLiteral("Line 1: \"scale\" ignored"),
                             QStringLiteral("Line 3: \"caption\" ignored") }));
}

TEST(ClassParserTest, ReportsAnEmptyDiagramWithoutALine)
{
    EXPECT_EQ(parseError(QStringLiteral("@startuml\ntitle Nothing\n@enduml\n")),
              QStringLiteral("The diagram has no classes"));
}

TEST(ClassParserTest, ReportsAnUnclosedBodyAtTheLastLine)
{
    EXPECT_EQ(parseError(QStringLiteral("class Machine {\n  x : int\n")),
              QStringLiteral("Line 2: the body of the class \"Machine\" opened at line 1 is not "
                             "closed with \"}\". Add \"}\" where the body ends."));
}

TEST(ClassParserTest, ReportsACloserWithoutABody)
{
    EXPECT_EQ(parseError(QStringLiteral("class A\n}\n")),
              QStringLiteral("Line 2: \"}\" without an open class body"));
}

TEST(ClassParserTest, ReportsAMalformedArrowAtItsLine)
{
    const QString error = parseError(QStringLiteral("class A\nclass B\nA <-=-- B\n"));

    EXPECT_TRUE(error.startsWith(QStringLiteral("Line 3: \"A <-=-- B\" is not a valid relation. "
                                                "Write it as: ")))
        << error.toStdString();
}

TEST(ClassParserTest, ReportsAnUnknownStatementAtItsLine)
{
    const QString error = parseError(QStringLiteral("class A\nwhatever now\n"));

    EXPECT_TRUE(error.startsWith(QStringLiteral("Line 2: unknown statement \"whatever now\". A "
                                                "class diagram here understands: ")))
        << error.toStdString();
}

TEST(ClassParserTest, NamesTheFormOfAKnownKeywordWrittenWrongly)
{
    EXPECT_TRUE(parseError(QStringLiteral("class A\nclass foo-bar\n"))
                    .startsWith(QStringLiteral("Line 2: \"class foo-bar\" is not a valid "
                                               "\"class\" statement. Write it as: ")));
}

TEST(ClassParserTest, ReportsADuplicateDeclarationAndAnUnknownNoteTarget)
{
    EXPECT_EQ(parseError(QStringLiteral("class A\nstruct A\n")),
              QStringLiteral("Line 2: the class \"A\" is already declared at line 1. Give each "
                             "class its own name."));
    EXPECT_EQ(parseError(QStringLiteral("class A\nnote left of B : x\n")),
              QStringLiteral("Line 2: unknown class \"B\" in the note. Declare it before the "
                             "note."));
}

TEST(ClassParserTest, RejectsPackagesAndNamespaces)
{
    expectUnsupported(QStringLiteral("package shop {\n  class A\n}\n"),
                      QStringLiteral("\"package\" and \"namespace\" blocks"));
    expectUnsupported(QStringLiteral("class A\nnamespace net {\n}\n"),
                      QStringLiteral("\"package\" and \"namespace\" blocks"));
}

TEST(ClassParserTest, RejectsStereotypes)
{
    expectUnsupported(QStringLiteral("class A <<entity>>\n"),
                      QStringLiteral("stereotypes such as <<entity>>"));
}

TEST(ClassParserTest, RejectsMemberModifiers)
{
    expectUnsupported(QStringLiteral("class A {\n  {static} count : int\n}\n"),
                      QStringLiteral("member modifiers such as {static} and {abstract}"));
    expectUnsupported(QStringLiteral("class A {\n  {abstract} run()\n}\n"),
                      QStringLiteral("member modifiers such as {static} and {abstract}"));
}

TEST(ClassParserTest, RejectsStyleDirectivesAndLayoutDirections)
{
    expectUnsupported(QStringLiteral("skinparam monochrome true\nclass A\n"),
                      QStringLiteral("skinparam"));
    expectUnsupported(QStringLiteral("hide members\nclass A\n"),
                      QStringLiteral("\"hide\" and \"show\""));
    expectUnsupported(QStringLiteral("show fields\nclass A\n"),
                      QStringLiteral("\"hide\" and \"show\""));
    expectUnsupported(QStringLiteral("left to right direction\nclass A\n"),
                      QStringLiteral("layout directions"));
}

TEST(ClassParserTest, RejectsDirectionHintsInArrows)
{
    expectUnsupported(QStringLiteral("class A\nclass B\nA -up-> B\n"),
                      QStringLiteral("direction hints in arrows"));
    expectUnsupported(QStringLiteral("class A\nclass B\nA -right- B\n"),
                      QStringLiteral("direction hints in arrows"));
}

TEST(ClassParserTest, RejectsLollipopsAssociationClassesAndTogether)
{
    expectUnsupported(QStringLiteral("class A\nA ()-- Port\n"),
                      QStringLiteral("interface lollipops such as ()--"));
    expectUnsupported(QStringLiteral("class A\nclass B\nclass C\n(A, B) .. C\n"),
                      QStringLiteral("association classes such as (A, B) .. C"));
    expectUnsupported(QStringLiteral("together {\n  class A\n}\n"),
                      QStringLiteral("\"together\" blocks"));
}

TEST(ClassParserTest, RejectsColours)
{
    expectUnsupported(QStringLiteral("class A #red\n"), QStringLiteral("colours"));
    expectUnsupported(QStringLiteral("class A\nclass B\nA -[#red]-> B\n"),
                      QStringLiteral("coloured or styled arrows"));
}

TEST(ClassParserTest, RejectsNotesThatAreNotDrawn)
{
    expectUnsupported(QStringLiteral("class A\nnote top of A : x\n"),
                      QStringLiteral("notes above or below a class"));
    expectUnsupported(QStringLiteral("class A\nfloating note : x\n"),
                      QStringLiteral("floating notes and notes on relations"));
}

TEST(ClassParserTest, ReportsTheLineOfAnUnsupportedElement)
{
    EXPECT_TRUE(parseError(QStringLiteral("class A\nclass B\npackage p {\n"))
                    .startsWith(QStringLiteral("Line 3: \"package p {\" is valid PlantUML, but "
                                               "this app does not support ")));
}

TEST(ClassParserTest, ReadsTheIndexOfEachClassInDeclarationOrder)
{
    const auto diagram = parsed(kKernel);

    EXPECT_EQ(indexOfType(diagram, QStringLiteral("mm_struct")), 1);
    EXPECT_EQ(indexOfType(diagram, QStringLiteral("files_struct")), 3);
}
