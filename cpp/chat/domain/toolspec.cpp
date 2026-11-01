#include "chat/domain/toolspec.h"

#include <QJsonArray>
#include <utility>

namespace chat
{

ToolSpec::ToolSpec(QString name, QString description)
    : m_name(std::move(name))
    , m_description(std::move(description))
{
}

ToolSpec& ToolSpec::string(QString name, QString description)
{
    return addParameter(std::move(name), std::move(description), QStringLiteral("string"), true);
}

ToolSpec& ToolSpec::integer(QString name, QString description)
{
    return addParameter(std::move(name), std::move(description), QStringLiteral("integer"), true);
}

ToolSpec& ToolSpec::optionalString(QString name, QString description)
{
    return addParameter(std::move(name), std::move(description), QStringLiteral("string"), false);
}

ToolSpec& ToolSpec::effect(ToolEffect toolEffect)
{
    m_effect = toolEffect;
    return *this;
}

ToolSpec& ToolSpec::handler(ToolHandler toolHandler)
{
    m_handler = std::move(toolHandler);
    return *this;
}

QJsonObject ToolSpec::inputSchema() const
{
    QJsonObject properties;
    QJsonArray required;
    for (const Parameter& parameter : m_parameters)
    {
        QJsonObject property;
        property.insert(QStringLiteral("type"), parameter.type);
        property.insert(QStringLiteral("description"), parameter.description);
        properties.insert(parameter.name, property);
        if (parameter.required)
        {
            required.append(parameter.name);
        }
    }

    QJsonObject schema;
    schema.insert(QStringLiteral("type"), QStringLiteral("object"));
    schema.insert(QStringLiteral("properties"), properties);
    schema.insert(QStringLiteral("additionalProperties"), false);
    if (!required.isEmpty())
    {
        schema.insert(QStringLiteral("required"), required);
    }
    return schema;
}

ToolSpec& ToolSpec::addParameter(QString name, QString description, QString type, bool required)
{
    m_parameters.push_back(
        Parameter { std::move(name), std::move(description), std::move(type), required });
    return *this;
}

}
