#pragma once

#include <QString>
#include <optional>
#include <vector>

#include "chat/domain/backendsession.h"
#include "chat/domain/message.h"

namespace chat
{

struct TranscriptLimits final
{
    int maxMessages { 0 };
    int maxChars { 0 };
};

inline constexpr TranscriptLimits kTranscriptLimits { 40, 30000 };
inline constexpr TranscriptLimits kSeedLimits { kTranscriptLimits.maxMessages / 2,
                                                kTranscriptLimits.maxChars / 2 };

inline const QString kOmittedMarker = QStringLiteral("(earlier conversation omitted)");

struct TurnTranscript final
{
    std::vector<Message> messages;
    int start { 0 };
    bool resumed { false };
};

bool fitsLimits(const std::vector<Message>& messages, const TranscriptLimits& limits);

TurnTranscript trimTranscript(const std::vector<Message>& messages, const TranscriptLimits& limits);

TurnTranscript planTranscript(const std::vector<Message>& messages,
                              const std::optional<BackendSession>& session);

}
