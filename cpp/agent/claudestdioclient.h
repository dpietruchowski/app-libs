#pragma once

#include <memory>

#include "claudelauncher.h"
#include "client.h"

class ClaudeStdioClient final : public Client
{
public:
    explicit ClaudeStdioClient(ClaudeLauncher launcher);
    ~ClaudeStdioClient() override;

    void createCompletionAsync(const ModelConfig& config, const Messages& messages,
                               const ToolsMap& toolsMap,
                               const CompletionCreatedCallback& callback) const override;
    void createCompletionAsync(const ModelConfig& config, const Messages& messages,
                               const ToolsMap& toolsMap, const CompletionCreatedCallback& callback,
                               const PartialTextCallback& partialText) const override;
    Completion createCompletion(const ModelConfig& config, const Messages& messages,
                                const ToolsMap& toolsMap) const override;

    static bool isAvailable();

private:
    class Run;

    std::shared_ptr<Run> m_run;
};
