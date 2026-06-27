#pragma once

#include <QByteArray>
#include <QString>

#include <functional>

class FileSaverQt final
{
public:
    using SavedCallback = std::function<void(const QString&)>;
    using CancelledCallback = std::function<void()>;
    using FailedCallback = std::function<void(const QString&)>;

    FileSaverQt(SavedCallback onSaved, CancelledCallback onCancelled, FailedCallback onFailed);

    void launch(const QString& suggestedName, const QString& mimeType, const QByteArray& data);

private:
    SavedCallback m_onSaved;
    CancelledCallback m_onCancelled;
    FailedCallback m_onFailed;
};
