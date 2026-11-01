#pragma once

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QPromise>
#include <memory>
#include <utility>
#include <vector>

#include "async/task.h"
#include "chat/application/agentcatalog.h"

namespace chat::test
{

class DeferredCalls
{
public:
    chat::ToolSpec tool(const QString& name)
    {
        chat::ToolSpec spec(name, "Answers when the test completes the call");
        spec.handler(
            [this](const QJsonObject&)
            {
                auto promise = std::make_shared<QPromise<Result<QJsonValue>>>();
                promise->start();
                m_pending.push_back(promise);
                return Task<QJsonValue>(nullptr, promise->future());
            });
        return spec;
    }

    size_t pending() const { return m_pending.size(); }

    void complete(size_t index, const QJsonValue& value)
    {
        finish(index, Result<QJsonValue>::success(value));
    }

    void fail(size_t index, const QString& error)
    {
        finish(index, Result<QJsonValue>::failure(error));
    }

private:
    void finish(size_t index, Result<QJsonValue> result)
    {
        m_pending.at(index)->addResult(std::move(result));
        m_pending.at(index)->finish();
        QCoreApplication::processEvents();
    }

    std::vector<std::shared_ptr<QPromise<Result<QJsonValue>>>> m_pending;
};

inline QJsonArray coursesPayload()
{
    return QJsonArray { QJsonObject { { "id", 1 }, { "title", "Networking" } },
                        QJsonObject { { "id", 2 }, { "title", "Security" } } };
}

inline chat::AgentCatalog makeCatalog(DeferredCalls& deferred)
{
    chat::ToolSpec list("courses_list", "Lists the courses");
    list.handler(
        [](const QJsonObject&) {
            return Task<QJsonValue>::ready(nullptr, Result<QJsonValue>::success(coursesPayload()));
        });

    chat::ToolSpec echo("courses_echo", "Returns the arguments it received");
    echo.string("text", "Text to return")
        .handler(
            [](const QJsonObject& arguments)
            { return Task<QJsonValue>::ready(nullptr, Result<QJsonValue>::success(arguments)); });

    chat::ToolSpec failing("courses_fail", "Always fails");
    failing.handler(
        [](const QJsonObject&)
        {
            return Task<QJsonValue>::ready(nullptr,
                                           Result<QJsonValue>::failure("Course store is offline"));
        });

    chat::ToolSpec readLesson("lessons_read", "Reads a lesson");
    readLesson.integer("lessonId", "Lesson to read")
        .handler(
            [](const QJsonObject&) {
                return Task<QJsonValue>::ready(nullptr, Result<QJsonValue>::success("Lesson text"));
            });

    chat::AgentCatalog catalog;
    catalog.add(
        chat::AgentDefinition { .key = "courses",
                                .title = "Courses",
                                .tools = { list, echo, failing, deferred.tool("courses_slow") } });
    catalog.add(
        chat::AgentDefinition { .key = "lessons", .title = "Lessons", .tools = { readLesson } });
    return catalog;
}

}
