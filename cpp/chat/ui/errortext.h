#pragma once

#include <QString>

#include "chat/domain/enums.h"

namespace chat
{

QString errorText(MessageStatus status, const QString& errorToken, const QString& loginCommand);

}
