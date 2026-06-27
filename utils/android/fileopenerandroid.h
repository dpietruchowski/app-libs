#pragma once

#include <QByteArray>
#include <QString>

#include <functional>

#include <QtCore/private/qandroidextras_p.h>

class FileOpenerAndroid final : public QAndroidActivityResultReceiver
{
public:
    using OpenedCallback = std::function<void(const QString&, const QByteArray&)>;
    using CancelledCallback = std::function<void()>;
    using FailedCallback = std::function<void(const QString&)>;

    FileOpenerAndroid(OpenedCallback onOpened, CancelledCallback onCancelled,
                      FailedCallback onFailed);

    static void setQmlEngine(class QQmlEngine*) { }

    void launch(const QString& mimeType = QString());

    void handleActivityResult(int receiverRequestCode, int resultCode,
                              const QJniObject& data) override;

private:
    OpenedCallback m_onOpened;
    CancelledCallback m_onCancelled;
    FailedCallback m_onFailed;
};