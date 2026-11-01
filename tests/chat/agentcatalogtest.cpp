#include <gtest/gtest.h>

#include <QJsonObject>
#include <QJsonValue>

#include "chat/application/agentcatalog.h"

namespace
{

Result<QJsonValue> await(Task<QJsonValue> task) { return task.future().result(); }

chat::ToolSpec echoTool(const QString& name, int& callCount)
{
    chat::ToolSpec tool(name, "Echoes the text argument");
    tool.string("text", "Text to echo")
        .handler(
            [&callCount](const QJsonObject& arguments)
            {
                ++callCount;
                return Task<QJsonValue>::ready(
                    nullptr, Result<QJsonValue>::success(arguments.value("text")));
            });
    return tool;
}

chat::AgentDefinition agentWith(const QString& key, const chat::ToolSpec& tool)
{
    return chat::AgentDefinition { .key = key, .title = key, .tools = { tool } };
}

}

class AgentCatalogTest : public ::testing::Test
{
protected:
    chat::AgentCatalog catalog;
    int callCount { 0 };
};

TEST_F(AgentCatalogTest, Invoke_KnownTool_PassesArgumentsAndReturnsHandlerValue)
{
    catalog.add(agentWith("courses", echoTool("echo", callCount)));

    const Result<QJsonValue> result
        = await(catalog.invoke("courses", "echo", QJsonObject { { "text", "hello" } }));

    ASSERT_TRUE(result.isSuccess());
    EXPECT_EQ(result.value().toString(), "hello");
    EXPECT_EQ(callCount, 1);
}

TEST_F(AgentCatalogTest, Invoke_UnknownAgent_FailsWithoutCallingAnyHandler)
{
    catalog.add(agentWith("courses", echoTool("echo", callCount)));

    const Result<QJsonValue> result = await(catalog.invoke("lessons", "echo", {}));

    ASSERT_TRUE(result.isFailure());
    EXPECT_TRUE(result.error().contains("lessons"));
    EXPECT_EQ(callCount, 0);
}

TEST_F(AgentCatalogTest, Invoke_UnknownTool_FailsWithoutCallingAnyHandler)
{
    catalog.add(agentWith("courses", echoTool("echo", callCount)));

    const Result<QJsonValue> result = await(catalog.invoke("courses", "shout", {}));

    ASSERT_TRUE(result.isFailure());
    EXPECT_TRUE(result.error().contains("shout"));
    EXPECT_EQ(callCount, 0);
}

TEST_F(AgentCatalogTest, Invoke_ToolWithoutHandler_Fails)
{
    catalog.add(agentWith("courses", chat::ToolSpec("echo", "No handler attached")));

    const Result<QJsonValue> result = await(catalog.invoke("courses", "echo", {}));

    ASSERT_TRUE(result.isFailure());
    EXPECT_TRUE(result.error().contains("echo"));
}

TEST_F(AgentCatalogTest, Invoke_ToolOfAnotherAgent_Fails)
{
    catalog.add(agentWith("courses", echoTool("echo", callCount)));
    catalog.add(agentWith("lessons", echoTool("read", callCount)));

    const Result<QJsonValue> result = await(catalog.invoke("lessons", "echo", {}));

    ASSERT_TRUE(result.isFailure());
    EXPECT_EQ(callCount, 0);
}

TEST_F(AgentCatalogTest, Invoke_ToolSharedByTwoAgents_ResolvesThroughBoth)
{
    const chat::ToolSpec shared = echoTool("echo", callCount);
    catalog.add(agentWith("courses", shared));
    catalog.add(agentWith("lessons", shared));

    const Result<QJsonValue> viaCourses
        = await(catalog.invoke("courses", "echo", QJsonObject { { "text", "a" } }));
    const Result<QJsonValue> viaLessons
        = await(catalog.invoke("lessons", "echo", QJsonObject { { "text", "b" } }));

    ASSERT_TRUE(viaCourses.isSuccess());
    ASSERT_TRUE(viaLessons.isSuccess());
    EXPECT_EQ(viaCourses.value().toString(), "a");
    EXPECT_EQ(viaLessons.value().toString(), "b");
    EXPECT_EQ(callCount, 2);
}

TEST_F(AgentCatalogTest, Add_SameKeyTwice_ReplacesTheFirstAgent)
{
    catalog.add(chat::AgentDefinition { .key = "courses", .title = "First" });
    catalog.add(chat::AgentDefinition { .key = "courses", .title = "Second" });

    ASSERT_EQ(catalog.agents().size(), 1u);
    EXPECT_EQ(catalog.find("courses")->title, "Second");
}

TEST_F(AgentCatalogTest, Add_DifferentKeys_KeepsInsertionOrder)
{
    catalog.add(chat::AgentDefinition { .key = "courses" });
    catalog.add(chat::AgentDefinition { .key = "lessons" });

    ASSERT_EQ(catalog.agents().size(), 2u);
    EXPECT_EQ(catalog.agents().at(0).key, "courses");
    EXPECT_EQ(catalog.agents().at(1).key, "lessons");
}

TEST_F(AgentCatalogTest, Find_UnknownKey_ReturnsNull)
{
    catalog.add(chat::AgentDefinition { .key = "courses" });

    EXPECT_EQ(catalog.find("lessons"), nullptr);
}
