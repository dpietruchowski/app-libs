#pragma once

#include <QByteArray>
#include <QString>

#include <functional>

#include <QtCore/private/qandroidextras_p.h>

class FileSaverAndroid final : public QAndroidActivityResultReceiver
{
public:
    using SavedCallback = std::function<void(const QString&)>;
    using CancelledCallback = std::function<void()>;
    using FailedCallback = std::function<void(const QString&)>;

    FileSaverAndroid(SavedCallback onSaved, CancelledCallback onCancelled, FailedCallback onFailed);

    void launch(const QString& suggestedName, const QString& mimeType, const QByteArray& data);

    void handleActivityResult(int receiverRequestCode, int resultCode,
                              const QJniObject& data) override;

private:
    SavedCallback m_onSaved;
    CancelledCallback m_onCancelled;
    FailedCallback m_onFailed;
    QByteArray m_pending;
};
