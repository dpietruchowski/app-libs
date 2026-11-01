#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <functional>
#include <vector>

#include "async/task.h"
#include "chat/domain/enums.h"

namespace chat
{

using ToolHandler = std::function<Task<QJsonValue>(const QJsonObject& arguments)>;

class ToolSpec final
{
public:
    ToolSpec(QString name, QString description);

    ToolSpec& string(QString name, QString description);
    ToolSpec& integer(QString name, QString description);
    ToolSpec& optionalString(QString name, QString description);
    ToolSpec& effect(ToolEffect toolEffect);
    ToolSpec& handler(ToolHandler toolHandler);

    const QString& name() const { return m_name; }
    const QString& description() const { return m_description; }
    ToolEffect effect() const { return m_effect; }
    const ToolHandler& handler() const { return m_handler; }

    QJsonObject inputSchema() const;

private:
    struct Parameter final
    {
        QString name;
        QString description;
        QString type;
        bool required { true };
    };

    ToolSpec& addParameter(QString name, QString description, QString type, bool required);

    QString m_name;
    QString m_description;
    ToolEffect m_effect { ToolEffect::Reads };
    ToolHandler m_handler;
    std::vector<Parameter> m_parameters;
};

}
