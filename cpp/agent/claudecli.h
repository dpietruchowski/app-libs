#pragma once

#include <QString>

#include "structures.h"

namespace claudecli
{

inline const QString kNetworkError = QStringLiteral("network_error");
inline const QString kServerError = QStringLiteral("server_error");
inline const QString kTimeout = QStringLiteral("timeout");

Completion failure(const QString& error);

QString executable();
QString modelOf(const ModelConfig& config);
QString effortOf(const ModelConfig& config);
QString systemMessages(const Messages& messages);
QString conversation(const Messages& messages);

}
