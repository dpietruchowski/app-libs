#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonObject>

#include "chat/domain/toolspec.h"

class ToolSpecTest : public ::testing::Test
{
};

TEST_F(ToolSpecTest, InputSchema_WithoutParameters_HasEmptyPropertiesAndNoRequired)
{
    const QJsonObject schema = chat::ToolSpec("ping", "Checks the connection").inputSchema();

    EXPECT_EQ(schema.value("type").toString(), "object");
    EXPECT_TRUE(schema.value("properties").toObject().isEmpty());
    EXPECT_FALSE(schema.contains("required"));
    EXPECT_FALSE(schema.value("additionalProperties").toBool(true));
}

TEST_F(ToolSpecTest, InputSchema_MixedParameters_DescribesEachTypeAndDescription)
{
    chat::ToolSpec spec("add_fact", "Adds a fact");
    spec.string("text", "The fact")
        .integer("courseNumber", "Course")
        .optionalString("note", "Hint");

    const QJsonObject properties = spec.inputSchema().value("properties").toObject();

    ASSERT_EQ(properties.size(), 3);
    EXPECT_EQ(properties.value("text").toObject().value("type").toString(), "string");
    EXPECT_EQ(properties.value("text").toObject().value("description").toString(), "The fact");
    EXPECT_EQ(properties.value("courseNumber").toObject().value("type").toString(), "integer");
    EXPECT_EQ(properties.value("courseNumber").toObject().value("description").toString(),
              "Course");
    EXPECT_EQ(properties.value("note").toObject().value("type").toString(), "string");
    EXPECT_EQ(properties.value("note").toObject().value("description").toString(), "Hint");
}

TEST_F(ToolSpecTest, InputSchema_MixedParameters_RequiresOnlyNonOptionalInDeclarationOrder)
{
    chat::ToolSpec spec("add_fact", "Adds a fact");
    spec.string("text", "The fact")
        .optionalString("note", "Hint")
        .integer("courseNumber", "Course");

    const QJsonArray required = spec.inputSchema().value("required").toArray();

    ASSERT_EQ(required.size(), 2);
    EXPECT_EQ(required.at(0).toString(), "text");
    EXPECT_EQ(required.at(1).toString(), "courseNumber");
}

TEST_F(ToolSpecTest, InputSchema_OnlyOptionalParameters_HasNoRequired)
{
    chat::ToolSpec spec("search", "Searches");
    spec.optionalString("query", "What to look for");

    EXPECT_FALSE(spec.inputSchema().contains("required"));
}

TEST_F(ToolSpecTest, Effect_DefaultsToReads)
{
    EXPECT_EQ(chat::ToolSpec("ping", "Checks").effect(), chat::ToolEffect::Reads);
}

TEST_F(ToolSpecTest, Effect_CanBeSetToWrites)
{
    chat::ToolSpec spec("remove_fact", "Removes a fact");
    spec.effect(chat::ToolEffect::Writes);

    EXPECT_EQ(spec.effect(), chat::ToolEffect::Writes);
}

TEST_F(ToolSpecTest, Handler_IsEmptyUntilSet)
{
    chat::ToolSpec spec("ping", "Checks");
    EXPECT_FALSE(spec.handler());

    spec.handler([](const QJsonObject&)
                 { return Task<QJsonValue>::ready(nullptr, Result<QJsonValue>::success({})); });

    EXPECT_TRUE(spec.handler());
}
