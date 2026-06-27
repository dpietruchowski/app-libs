#pragma once

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>

class QQmlEngine;

class FileOpenerQt : public QObject
{
    Q_OBJECT

public:
    using OpenedCallback = std::function<void(const QString&, const QByteArray&)>;
    using CancelledCallback = std::function<void()>;
    using FailedCallback = std::function<void(const QString&)>;

    FileOpenerQt(OpenedCallback onOpened, CancelledCallback onCancelled, FailedCallback onFailed);

    static void setQmlEngine(QQmlEngine* engine);

    void launch(const QString& mimeType = QString());

private slots:
    void onFileSelected(const QString& path);
    void onDialogClosed();

    void read(const QString& path);

private:
    static QQmlEngine* s_engine;

    OpenedCallback m_onOpened;
    CancelledCallback m_onCancelled;
    FailedCallback m_onFailed;
    QPointer<QObject> m_dialog;
    bool m_selected { false };
};
