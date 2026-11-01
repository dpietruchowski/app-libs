#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QUuid>
#include <algorithm>
#include <optional>
#include <vector>

#include "agent/tooleffect.h"
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
    ToolEffect effect { ToolEffect::Reads };

    bool operator==(const Step& other) const = default;
};

inline void upsertStep(std::vector<Step>& steps, const Step& step)
{
    const auto known = std::find_if(steps.begin(), steps.end(),
                                    [&step](const Step& s) { return s.id == step.id; });
    if (known == steps.end())
    {
        steps.push_back(step);
    }
    else
    {
        *known = step;
    }
}

}
