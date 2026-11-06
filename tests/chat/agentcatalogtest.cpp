#include <gtest/gtest.h>

#include "chat/application/agentcatalog.h"

namespace
{

chat::AppAgent appAgent(const QString& key,
                        const QString& description = QStringLiteral("Does the work."))
{
    return chat::AppAgent { key, key.toUpper(), description,
                            ::Agent(QStringLiteral("model"), QStringLiteral("prompt")) };
}

}

TEST(AgentCatalogTest, Add_AValidAgent_IsStoredAndFoundByItsKey)
{
    chat::AgentCatalog catalog;

    EXPECT_TRUE(catalog.add(appAgent(QStringLiteral("courses"))));

    ASSERT_EQ(catalog.agents().size(), 1u);
    const chat::AppAgent* found = catalog.find(QStringLiteral("courses"));
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->title, QStringLiteral("COURSES"));
    EXPECT_EQ(found->description, QStringLiteral("Does the work."));
}

TEST(AgentCatalogTest, Add_KeepsTheOrderOfRegistration)
{
    chat::AgentCatalog catalog;

    catalog.add(appAgent(QStringLiteral("courses")));
    catalog.add(appAgent(QStringLiteral("facts")));
    catalog.add(appAgent(QStringLiteral("progress")));

    ASSERT_EQ(catalog.agents().size(), 3u);
    EXPECT_EQ(catalog.agents().at(0).key, QStringLiteral("courses"));
    EXPECT_EQ(catalog.agents().at(1).key, QStringLiteral("facts"));
    EXPECT_EQ(catalog.agents().at(2).key, QStringLiteral("progress"));
}

TEST(AgentCatalogTest, Add_ARepeatedKey_IsRejectedAndKeepsTheFirstAgent)
{
    chat::AgentCatalog catalog;
    catalog.add(appAgent(QStringLiteral("courses"), QStringLiteral("First.")));

    EXPECT_FALSE(catalog.add(appAgent(QStringLiteral("courses"), QStringLiteral("Second."))));

    ASSERT_EQ(catalog.agents().size(), 1u);
    EXPECT_EQ(catalog.find(QStringLiteral("courses"))->description, QStringLiteral("First."));
}

TEST(AgentCatalogTest, Add_AKeyThatIsNotAToolName_IsRejected)
{
    chat::AgentCatalog catalog;

    EXPECT_FALSE(catalog.add(appAgent(QString())));
    EXPECT_FALSE(catalog.add(appAgent(QStringLiteral("two words"))));
    EXPECT_FALSE(catalog.add(appAgent(QStringLiteral("with-dash"))));
    EXPECT_FALSE(catalog.add(appAgent(QStringLiteral("courses\n"))));

    EXPECT_TRUE(catalog.agents().empty());
}

TEST(AgentCatalogTest, Add_AKeyWithUnderscoresAndDigits_IsAccepted)
{
    chat::AgentCatalog catalog;

    EXPECT_TRUE(catalog.add(appAgent(QStringLiteral("fact_checker2"))));
}

TEST(AgentCatalogTest, Add_ABlankDescription_IsRejected)
{
    chat::AgentCatalog catalog;

    EXPECT_FALSE(catalog.add(appAgent(QStringLiteral("courses"), QString())));
    EXPECT_FALSE(catalog.add(appAgent(QStringLiteral("courses"), QStringLiteral(" \n "))));

    EXPECT_TRUE(catalog.agents().empty());
}

TEST(AgentCatalogTest, ModelFor_AChosenModel_WinsOverTheOwnModelAndTheChat)
{
    chat::AppAgent agent = appAgent(QStringLiteral("courses"));
    agent.config.model = QStringLiteral("own");

    EXPECT_EQ(agent.modelFor(QStringLiteral("chosen"), QStringLiteral("chat")),
              QStringLiteral("chosen"));
}

TEST(AgentCatalogTest, ModelFor_WithoutAChoice_IsTheOwnModel)
{
    chat::AppAgent agent = appAgent(QStringLiteral("courses"));
    agent.config.model = QStringLiteral("own");

    EXPECT_EQ(agent.modelFor(QString(), QStringLiteral("chat")), QStringLiteral("own"));
}

TEST(AgentCatalogTest, ModelFor_WithoutAChoiceOrAnOwnModel_IsTheModelOfTheChat)
{
    const chat::AppAgent agent = appAgent(QStringLiteral("courses"));

    EXPECT_EQ(agent.modelFor(QString(), QStringLiteral("chat")), QStringLiteral("chat"));
}

TEST(AgentCatalogTest, Find_AnUnknownKey_ReturnsNothing)
{
    chat::AgentCatalog catalog;
    catalog.add(appAgent(QStringLiteral("courses")));

    EXPECT_EQ(catalog.find(QStringLiteral("facts")), nullptr);
}
