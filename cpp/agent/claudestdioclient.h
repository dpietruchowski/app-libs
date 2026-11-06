#pragma once

#include <memory>

#include "claudelauncher.h"
#include "claudesession.h"
#include "client.h"

class ClaudeStdioClient final : public Client
{
public:
    explicit ClaudeStdioClient(ClaudeLauncher launcher, ClaudeSession session = {});
    ~ClaudeStdioClient() override;

    void createCompletionAsync(const ModelConfig& config, const Messages& messages,
                               const ToolsMap& toolsMap,
                               const CompletionCreatedCallback& callback) const override;
    void createCompletionAsync(const ModelConfig& config, const Messages& messages,
                               const ToolsMap& toolsMap, const CompletionCreatedCallback& callback,
                               const PartialTextCallback& partialText) const override;
    Completion createCompletion(const ModelConfig& config, const Messages& messages,
                                const ToolsMap& toolsMap) const override;

    QString session() const override;

    static bool isAvailable();
    static void forget(const ClaudeLauncher& launcher, const QString& id);

private:
    class Run;

    std::shared_ptr<Run> m_run;
};
