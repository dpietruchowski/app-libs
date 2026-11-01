#include "chat/application/transcript.h"

#include <algorithm>
#include <cstddef>
#include <iterator>

namespace chat
{

namespace
{

    std::vector<Message> completeMessages(const std::vector<Message>& messages)
    {
        std::vector<Message> complete;
        std::copy_if(messages.begin(), messages.end(), std::back_inserter(complete),
                     [](const Message& message)
                     { return message.status() == MessageStatus::Complete; });
        return complete;
    }

    std::size_t firstAtOrAfter(const std::vector<Message>& messages, int position)
    {
        const auto found
            = std::find_if(messages.begin(), messages.end(), [position](const Message& message)
                           { return message.position() >= position; });
        return static_cast<std::size_t>(std::distance(messages.begin(), found));
    }

    std::vector<Message> tailFrom(const std::vector<Message>& messages, std::size_t first)
    {
        const std::size_t clamped = std::min(first, messages.size());
        return std::vector<Message>(messages.begin() + static_cast<std::ptrdiff_t>(clamped),
                                    messages.end());
    }

    std::size_t firstKeptIndex(const std::vector<Message>& messages, const TranscriptLimits& limits)
    {
        std::size_t first = messages.size();
        int chars = 0;
        while (first > 0)
        {
            const Message& candidate = messages[first - 1];
            const int count = static_cast<int>(messages.size() - first) + 1;
            const int total = chars + static_cast<int>(candidate.text().size());
            const bool alreadyKeptOne = first < messages.size();
            if (alreadyKeptOne && (count > limits.maxMessages || total > limits.maxChars))
            {
                break;
            }
            chars = total;
            --first;
        }
        return first;
    }

    Message omittedMarker(int firstKeptPosition)
    {
        Message::Data data;
        data.position = firstKeptPosition - 1;
        data.role = Role::User;
        data.text = kOmittedMarker;
        data.status = MessageStatus::Complete;
        return Message(QUuid(), data, QDateTime());
    }

    std::vector<Message> windowFrom(const std::vector<Message>& messages, std::size_t first)
    {
        std::vector<Message> window = tailFrom(messages, first);
        if (first > 0 && !window.empty())
        {
            window.insert(window.begin(), omittedMarker(window.front().position()));
        }
        return window;
    }

}

bool fitsLimits(const std::vector<Message>& messages, const TranscriptLimits& limits)
{
    int chars = 0;
    for (const Message& message : messages)
    {
        chars += static_cast<int>(message.text().size());
    }
    return static_cast<int>(messages.size()) <= limits.maxMessages && chars <= limits.maxChars;
}

TurnTranscript trimTranscript(const std::vector<Message>& messages, const TranscriptLimits& limits)
{
    TurnTranscript transcript;
    const std::size_t first = firstKeptIndex(messages, limits);
    if (first >= messages.size())
    {
        return transcript;
    }

    transcript.start = messages[first].position();
    transcript.messages = windowFrom(messages, first);
    return transcript;
}

TurnTranscript planTranscript(const std::vector<Message>& messages,
                              const std::optional<BackendSession>& session)
{
    const std::vector<Message> history = completeMessages(messages);

    if (session)
    {
        const std::size_t first = firstAtOrAfter(history, session->start);
        if (fitsLimits(tailFrom(history, first), kTranscriptLimits))
        {
            TurnTranscript resumed;
            resumed.messages = windowFrom(history, first);
            resumed.start = session->start;
            resumed.resumed = true;
            return resumed;
        }
    }

    return trimTranscript(history,
                          fitsLimits(history, kTranscriptLimits) ? kTranscriptLimits : kSeedLimits);
}

}
