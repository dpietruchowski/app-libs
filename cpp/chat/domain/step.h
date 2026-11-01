#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QUuid>
#include <optional>

#include "chat/domain/enums.h"

namespace chat
{

struct Step final
{
    QUuid id;
    std::optional<QUuid> parentId;
    StepKind kind { StepKind::Delegation };
    QString agent;
    QString tool;
    QJsonObject input;
    QJsonValue output;
    StepStatus status { StepStatus::Running };

    bool operator==(const Step& other) const = default;
};

}
