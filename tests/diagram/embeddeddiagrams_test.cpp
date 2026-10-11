#include <gtest/gtest.h>

#include "diagram/embeddeddiagrams.h"

using namespace diagram;

TEST(EmbeddedDiagramsTest, FindsABlockWithItsBodyLineAndSpan)
{
    const QString text
        = QStringLiteral("<p>Intro</p>\n  @startuml\nA -> B : x\nB --> A\n@enduml\n<p>"
                         "Outro</p>");
    const QList<EmbeddedDiagram> diagrams = EmbeddedDiagrams::find(text);

    ASSERT_EQ(diagrams.size(), 1);
    const EmbeddedDiagram& diagram = diagrams.first();
    EXPECT_TRUE(diagram.closed);
    EXPECT_EQ(diagram.line, 1);
    EXPECT_EQ(diagram.text, QStringLiteral("A -> B : x\nB --> A"));
    EXPECT_EQ(text.left(diagram.start), QStringLiteral("<p>Intro</p>\n"));
    EXPECT_EQ(text.mid(diagram.end), QStringLiteral("\n<p>Outro</p>"));
}

TEST(EmbeddedDiagramsTest, FindsSeveralBlocksAndIgnoresMarkersInsideALine)
{
    const QString text = QStringLiteral("<p>say @startuml here</p>\n@StartUml\nA -> B\n@EndUml\n"
                                        "@startuml\n@enduml");
    const QList<EmbeddedDiagram> diagrams = EmbeddedDiagrams::find(text);

    ASSERT_EQ(diagrams.size(), 2);
    EXPECT_EQ(diagrams.at(0).text, QStringLiteral("A -> B"));
    EXPECT_EQ(diagrams.at(1).line, 4);
    EXPECT_TRUE(diagrams.at(1).text.isEmpty());
    EXPECT_TRUE(diagrams.at(1).closed);
    EXPECT_EQ(diagrams.at(1).end, text.size());
}

TEST(EmbeddedDiagramsTest, AnUnclosedBlockRunsToTheEnd)
{
    const QString text = QStringLiteral("<p>x</p>\n@startuml\nA -> B\n");
    const QList<EmbeddedDiagram> diagrams = EmbeddedDiagrams::find(text);

    ASSERT_EQ(diagrams.size(), 1);
    EXPECT_FALSE(diagrams.first().closed);
    EXPECT_EQ(diagrams.first().text, QStringLiteral("A -> B\n"));
    EXPECT_EQ(diagrams.first().end, text.size());
}

TEST(EmbeddedDiagramsTest, TheBodyLosesCarriageReturnsAndHtmlEscapes)
{
    const QString text
        = QStringLiteral("@startuml\r\nA -&gt; B : x &amp;lt; y\r\nB --&gt; A : &quot;ok&quot;\r\n"
                         "@enduml\r\n");
    const QList<EmbeddedDiagram> diagrams = EmbeddedDiagrams::find(text);

    ASSERT_EQ(diagrams.size(), 1);
    EXPECT_TRUE(diagrams.first().closed);
    EXPECT_EQ(diagrams.first().text, QStringLiteral("A -> B : x &lt; y\nB --> A : \"ok\""));
}

TEST(EmbeddedDiagramsTest, ReportsMarkersThatDoNotStartTheirLine)
{
    const QString text = QStringLiteral("<p>x</p>\n<p>@startuml\nA -> B\n@enduml</p>\n<pre>"
                                        "@EndUml</pre>\n@startuml\n@enduml");

    EXPECT_EQ(EmbeddedDiagrams::strayMarkers(text), (QList<int> { 1, 3, 4 }));
}

TEST(EmbeddedDiagramsTest, TextWithoutBlocksHasNone)
{
    EXPECT_TRUE(EmbeddedDiagrams::find(QStringLiteral("<p>plain</p>")).isEmpty());
    EXPECT_TRUE(EmbeddedDiagrams::find(QString()).isEmpty());
}

TEST(EmbeddedDiagramsTest, LocatesAParseErrorInTheSurroundingText)
{
    EmbeddedDiagram diagram;
    diagram.line = 3;

    const LocatedError numbered = EmbeddedDiagrams::locate(
        diagram, QStringLiteral("Line 2: \"end\" without an open group"));
    EXPECT_EQ(numbered.line, 5);
    EXPECT_EQ(numbered.message, QStringLiteral("\"end\" without an open group"));

    const LocatedError whole = EmbeddedDiagrams::locate(diagram, QStringLiteral("no participants"));
    EXPECT_EQ(whole.line, 3);
    EXPECT_EQ(whole.message, QStringLiteral("no participants"));
}
