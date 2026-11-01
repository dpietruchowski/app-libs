#include "chatclaude/streamjsonparser.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QUuid>
#include <utility>

#include "chat/application/agentbackend.h"

namespace chat::claude
{

namespace
{

    const QString kDelegationTool = QStringLiteral("Agent");
    const QString kToolPrefix = QStringLiteral("mcp__app__");

    StreamEvent makeEvent(StreamEvent::Kind kind, Step step = {}, QString text = {},
                          QString detail = {})
    {
        StreamEvent event;
        event.kind = kind;
        event.step = std::move(step);
        event.text = std::move(text);
        event.detail = std::move(detail);
        return event;
    }

    QString toolNameOf(const QString& name)
    {
        return name.startsWith(kToolPrefix) ? name.mid(kToolPrefix.size()) : name;
    }

    QString textOf(const QJsonValue& content)
    {
        if (content.isString())
            return content.toString();

        QStringList parts;
        for (const QJsonValue& block : content.toArray())
        {
            const QJsonObject object = block.toObject();
            if (object.value("type").toString() == QStringLiteral("text"))
                parts.append(object.value("text").toString());
        }
        return parts.join(QLatin1Char('\n'));
    }

    QStringList stringsOf(const QJsonValue& value)
    {
        QStringList strings;
        for (const QJsonValue& item : value.toArray())
            strings.append(item.toString());
        return strings;
    }

}

std::vector<StreamEvent> StreamJsonParser::feed(const QByteArray& chunk)
{
    std::vector<StreamEvent> events;
    if (m_finished)
        return events;

    m_buffer += chunk;
    qsizetype lineStart = 0;
    while (!m_finished)
    {
        const qsizetype lineEnd = m_buffer.indexOf('\n', lineStart);
        if (lineEnd < 0)
            break;
        handleLine(m_buffer.sliced(lineStart, lineEnd - lineStart), events);
        lineStart = lineEnd + 1;
    }

    if (m_finished)
        m_buffer.clear();
    else
        m_buffer.remove(0, lineStart);
    return events;
}

std::vector<StreamEvent> StreamJsonParser::endOfStream()
{
    std::vector<StreamEvent> events;
    if (m_finished)
        return events;

    handleLine(m_buffer, events);
    m_buffer.clear();
    if (!m_finished)
    {
        conclude(makeEvent(StreamEvent::Kind::Failure, {}, turn_error::kServerError,
                           QStringLiteral("stream ended without a result")),
                 events);
    }
    return events;
}

bool StreamJsonParser::isFinished() const { return m_finished; }

const QStringList& StreamJsonParser::agents() const { return m_agents; }

void StreamJsonParser::handleLine(const QByteArray& line, std::vector<StreamEvent>& events)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return;

    const QJsonObject event = document.object();
    const QString type = event.value("type").toString();
    if (type == QStringLiteral("system"))
        handleSystem(event, events);
    else if (type == QStringLiteral("assistant"))
        handleAssistant(event, events);
    else if (type == QStringLiteral("user"))
        handleUser(event, events);
    else if (type == QStringLiteral("result"))
        handleResult(event, events);
}

void StreamJsonParser::handleSystem(const QJsonObject& event, std::vector<StreamEvent>& events)
{
    const QString subtype = event.value("subtype").toString();
    if (subtype == QStringLiteral("init"))
        handleInit(event, events);
    else if (subtype == QStringLiteral("task_notification"))
        handleTaskNotification(event, events);
}

void StreamJsonParser::handleInit(const QJsonObject& event, std::vector<StreamEvent>& events)
{
    m_agents = stringsOf(event.value("agents"));

    const QString sessionId = event.value("session_id").toString();
    if (!m_sessionReported && !sessionId.isEmpty())
    {
        m_sessionReported = true;
        events.push_back(makeEvent(StreamEvent::Kind::SessionStarted, {}, sessionId));
    }
}

void StreamJsonParser::handleTaskNotification(const QJsonObject& event,
                                              std::vector<StreamEvent>& events)
{
    const QString status = event.value("status").toString();
    const QString summary = event.value("summary").toString();
    const QString toolUseId = event.value("tool_use_id").toString();

    if (status == QStringLiteral("completed"))
        finishStep(toolUseId, StepStatus::Ok, summary, events);
    else if (status == QStringLiteral("failed"))
        finishStep(toolUseId, StepStatus::Failed, summary, events);
}

void StreamJsonParser::handleAssistant(const QJsonObject& event, std::vector<StreamEvent>& events)
{
    const QString parentToolUseId = event.value("parent_tool_use_id").toString();
    const QJsonArray content = event.value("message").toObject().value("content").toArray();
    for (const QJsonValue& block : content)
    {
        const QJsonObject object = block.toObject();
        if (object.value("type").toString() == QStringLiteral("tool_use"))
            startStep(object, parentToolUseId, events);
    }
}

void StreamJsonParser::handleUser(const QJsonObject& event, std::vector<StreamEvent>& events)
{
    const QJsonArray content = event.value("message").toObject().value("content").toArray();
    for (const QJsonValue& block : content)
    {
        const QJsonObject object = block.toObject();
        if (object.value("type").toString() != QStringLiteral("tool_result"))
            continue;

        const StepStatus status
            = object.value("is_error").toBool() ? StepStatus::Failed : StepStatus::Ok;
        finishStep(object.value("tool_use_id").toString(), status, textOf(object.value("content")),
                   events);
    }
}

void StreamJsonParser::handleResult(const QJsonObject& event, std::vector<StreamEvent>& events)
{
    const QString text = event.value("result").toString();
    const bool succeeded = event.value("subtype").toString() == QStringLiteral("success")
        && !event.value("is_error").toBool();

    conclude(succeeded ? makeEvent(StreamEvent::Kind::Answer, {}, text)
                       : makeEvent(StreamEvent::Kind::Failure, {}, turn_error::kServerError, text),
             events);
}

void StreamJsonParser::startStep(const QJsonObject& toolUse, const QString& parentToolUseId,
                                 std::vector<StreamEvent>& events)
{
    const QString toolUseId = toolUse.value("id").toString();
    if (toolUseId.isEmpty() || m_steps.contains(toolUseId))
        return;

    const QString name = toolUse.value("name").toString();
    const QJsonObject input = toolUse.value("input").toObject();
    const auto parent = m_steps.constFind(parentToolUseId);

    Step step;
    step.id = QUuid::createUuid();
    if (parent != m_steps.constEnd())
        step.parentId = parent->id;
    step.input = input;

    if (name == kDelegationTool)
    {
        step.kind = StepKind::Delegation;
        step.agent = input.value("subagent_type").toString();
    }
    else
    {
        step.kind = StepKind::Tool;
        step.tool = toolNameOf(name);
        if (parent != m_steps.constEnd())
            step.agent = parent->agent;
    }

    m_steps.insert(toolUseId, step);
    m_startOrder.append(toolUseId);
    events.push_back(makeEvent(StreamEvent::Kind::StepStarted, step));
}

void StreamJsonParser::finishStep(const QString& toolUseId, StepStatus status,
                                  const QJsonValue& output, std::vector<StreamEvent>& events)
{
    const auto found = m_steps.find(toolUseId);
    if (found == m_steps.end() || found->status != StepStatus::Running)
        return;

    found->status = status;
    found->output = output;
    events.push_back(makeEvent(StreamEvent::Kind::StepFinished, *found));
}

void StreamJsonParser::conclude(StreamEvent finalEvent, std::vector<StreamEvent>& events)
{
    closeRunningSteps(events);
    events.push_back(std::move(finalEvent));
    m_finished = true;
}

void StreamJsonParser::closeRunningSteps(std::vector<StreamEvent>& events)
{
    for (auto toolUseId = m_startOrder.crbegin(); toolUseId != m_startOrder.crend(); ++toolUseId)
        finishStep(*toolUseId, StepStatus::Failed, QJsonValue(), events);
}

}
