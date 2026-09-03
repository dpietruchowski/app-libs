#pragma once

#include "client.h"

class ClaudeCliClient final : public Client
{
public:
    ClaudeCliClient();

    void createCompletionAsync(const ModelConfig& config, const Messages& messages,
                               const ToolsMap& toolsMap,
                               const CompletionCreatedCallback& callback) const override;
    Completion createCompletion(const ModelConfig& config, const Messages& messages,
                                const ToolsMap& toolsMap) const override;

    static bool isAvailable();
    static QString executable();
};
