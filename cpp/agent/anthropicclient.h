#pragma once

#include <QJsonObject>
#include <QString>

#include "client.h"
#include "structures.h"

namespace anthropic
{
QJsonObject requestBody(const ModelConfig& config, const Messages& messages,
                        const ToolsMap& toolsMap);
Completion completionFrom(const QJsonObject& response);
}

class AnthropicClient final : public Client
{
public:
    explicit AnthropicClient(const QString& apiKey,
                             const QString& url
                             = QStringLiteral("https://api.anthropic.com/v1/messages"));

    void createCompletionAsync(const ModelConfig& config, const Messages& messages,
                               const ToolsMap& toolsMap,
                               const CompletionCreatedCallback& callback) const override;
    void createCompletionAsync(const ModelConfig& config, const Messages& messages,
                               const ToolsMap& toolsMap, const CompletionCreatedCallback& callback,
                               const PartialTextCallback& partialText) const override;
    Completion createCompletion(const ModelConfig& config, const Messages& messages,
                                const ToolsMap& toolsMap) const override;

private:
    QNetworkReply* post(const ModelConfig& config, const Messages& messages,
                        const ToolsMap& toolsMap) const;
    Completion parse(QNetworkReply* reply) const;
};
