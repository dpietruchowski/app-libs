#pragma once

#include <QByteArray>
#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>

class QQmlEngine;

class FileSaverQt : public QObject
{
    Q_OBJECT

public:
    using SavedCallback = std::function<void(const QString&)>;
    using CancelledCallback = std::function<void()>;
    using FailedCallback = std::function<void(const QString&)>;

    FileSaverQt(SavedCallback onSaved, CancelledCallback onCancelled, FailedCallback onFailed);

    static void setQmlEngine(QQmlEngine* engine);

    void launch(const QString& suggestedName, const QString& mimeType, const QByteArray& data);

private slots:
    void onFileAccepted(const QString& path);
    void onDialogClosed();

private:
    void write(const QString& path);

    static QQmlEngine* s_engine;

    SavedCallback m_onSaved;
    CancelledCallback m_onCancelled;
    FailedCallback m_onFailed;
    QPointer<QObject> m_dialog;
    QByteArray m_pending;
    bool m_accepted { false };
};
