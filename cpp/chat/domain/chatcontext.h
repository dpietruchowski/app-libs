#pragma once

#include <QJsonObject>
#include <QString>

namespace chat
{

struct ChatContext final
{
    QString key;
    QString title;
    QJsonObject data;

    bool operator==(const ChatContext& other) const = default;
};

}
