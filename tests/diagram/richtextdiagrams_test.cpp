#include <QSignalSpy>
#include <gtest/gtest.h>

#include "diagram/richtextdiagrams.h"

using namespace diagram;

namespace
{
const QString kDiagramHtml
    = QStringLiteral("<p>Before</p>\n@startuml\nA -> B : hello\n@enduml\n<p>After</p>");

int countImages(const QString& html)
{
    return int(html.count(QStringLiteral("<img src=\"data:image/svg+xml;utf8,")));
}

QString sizeOfTheHelloDiagram(int width)
{
    DiagramSvgProvider provider;
    provider.setAvailableWidth(width);
    provider.setText(QStringLiteral("A -> B : hello"));
    return QStringLiteral(" width=\"%1\" height=\"%2\">")
        .arg(provider.imageWidth())
        .arg(provider.imageHeight());
}
}

TEST(RichTextDiagramsTest, ReplacesABlockWithASizedImageAndKeepsTheRest)
{
    RichTextDiagrams diagrams;
    diagrams.setAvailableWidth(360);
    diagrams.setHtml(kDiagramHtml);

    const QString html = diagrams.renderedHtml();
    EXPECT_TRUE(html.startsWith(
        QStringLiteral("<p>Before</p>\n<p class=\"diagram\" align=\"center\"><img src=")));
    EXPECT_TRUE(html.endsWith(QStringLiteral("></p>\n<p>After</p>")));
    EXPECT_EQ(countImages(html), 1);
    EXPECT_FALSE(html.contains(QStringLiteral("@startuml")));
    EXPECT_TRUE(html.contains(sizeOfTheHelloDiagram(360)));
}

TEST(RichTextDiagramsTest, ShowsNothingInPlaceOfABlockUntilTheWidthIsKnown)
{
    RichTextDiagrams diagrams;
    diagrams.setHtml(kDiagramHtml);

    EXPECT_EQ(diagrams.renderedHtml(), QStringLiteral("<p>Before</p>\n\n<p>After</p>"));
}

TEST(RichTextDiagramsTest, HtmlWithoutBlocksPassesThrough)
{
    RichTextDiagrams diagrams;
    diagrams.setAvailableWidth(360);
    diagrams.setHtml(QStringLiteral("<p>a -> b</p>"));

    EXPECT_EQ(diagrams.renderedHtml(), QStringLiteral("<p>a -> b</p>"));
}

TEST(RichTextDiagramsTest, RendersEveryBlock)
{
    RichTextDiagrams diagrams;
    diagrams.setAvailableWidth(360);
    diagrams.setHtml(kDiagramHtml + QStringLiteral("\n@startuml\nC -> D\n@enduml"));

    EXPECT_EQ(countImages(diagrams.renderedHtml()), 2);
}

TEST(RichTextDiagramsTest, ShowsTheParseErrorAndAnUnclosedBlock)
{
    RichTextDiagrams diagrams;
    diagrams.setAvailableWidth(360);
    diagrams.setHtml(QStringLiteral("@startuml\nA -> B\nend\n@enduml\n@startuml\nA -> <B>\n"));

    EXPECT_EQ(diagrams.renderedHtml(),
              QStringLiteral("<p class=\"diagramError\">Diagram: &quot;end&quot; without an open "
                             "group</p>\n<p class=\"diagramError\">Diagram: @startuml without "
                             "@enduml</p>"));
}

TEST(RichTextDiagramsTest, ColoursAndWidthReachTheImage)
{
    RichTextDiagrams diagrams;
    diagrams.setAvailableWidth(360);
    diagrams.setHtml(kDiagramHtml);
    QSignalSpy changed(&diagrams, &RichTextDiagrams::renderedHtmlChanged);

    diagrams.setColors({ { QStringLiteral("text"), QStringLiteral("#123456") } });
    EXPECT_EQ(changed.count(), 1);
    EXPECT_TRUE(diagrams.renderedHtml().contains(QStringLiteral("%23123456")));

    diagrams.setAvailableWidth(240);
    EXPECT_EQ(changed.count(), 2);
    EXPECT_TRUE(diagrams.renderedHtml().contains(sizeOfTheHelloDiagram(240)));
    EXPECT_NE(sizeOfTheHelloDiagram(240), sizeOfTheHelloDiagram(360));
}

TEST(RichTextDiagramsTest, AnUnchangedDiagramIsNotRenderedAgain)
{
    RichTextDiagrams diagrams;
    diagrams.setAvailableWidth(360);
    diagrams.setHtml(kDiagramHtml);
    const QString first = diagrams.renderedHtml();
    QSignalSpy changed(&diagrams, &RichTextDiagrams::renderedHtmlChanged);

    diagrams.setHtml(kDiagramHtml + QStringLiteral(" "));

    EXPECT_EQ(changed.count(), 1);
    EXPECT_EQ(diagrams.renderedHtml(), first + QStringLiteral(" "));
}
