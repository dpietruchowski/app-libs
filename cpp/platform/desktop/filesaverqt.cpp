#include "filesaverqt.h"

#include <QFile>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QWindow>

QQmlEngine* FileSaverQt::s_engine = nullptr;

namespace
{
QStringList filtersForMimeType(const QString& mimeType)
{
    if (mimeType == QStringLiteral("application/json"))
        return { QStringLiteral("*.json") };
    if (mimeType == QStringLiteral("text/plain"))
        return { QStringLiteral("*.txt") };
    return { };
}

QQuickWindow* findQuickWindow()
{
    const auto windows = QGuiApplication::topLevelWindows();
    for (QWindow* window : windows)
    {
        if (auto* quickWindow = qobject_cast<QQuickWindow*>(window))
            return quickWindow;
    }
    return nullptr;
}
}  // namespace

void FileSaverQt::setQmlEngine(QQmlEngine* engine) { s_engine = engine; }

FileSaverQt::FileSaverQt(SavedCallback onSaved, CancelledCallback onCancelled,
                         FailedCallback onFailed)
    : m_onSaved(std::move(onSaved))
    , m_onCancelled(std::move(onCancelled))
    , m_onFailed(std::move(onFailed))
{
}

void FileSaverQt::launch(const QString& suggestedName, const QString& mimeType,
                         const QByteArray& data)
{
    if (m_dialog)
        return;

    if (!s_engine)
    {
        if (m_onFailed)
            m_onFailed(QStringLiteral("No QML engine available"));
        return;
    }

    QQuickWindow* window = findQuickWindow();
    if (!window)
    {
        if (m_onFailed)
            m_onFailed(QStringLiteral("No application window available"));
        return;
    }

    QQmlComponent component(s_engine);
    component.setData("import Themed.Components\nThemedFileSaveDialog {}", QUrl());
    if (component.isError())
    {
        if (m_onFailed)
            m_onFailed(component.errorString());
        return;
    }

    QObject* dialog = component.create();
    if (!dialog)
    {
        if (m_onFailed)
            m_onFailed(QStringLiteral("Could not create file dialog"));
        return;
    }

    dialog->setParent(window);
    dialog->setProperty("parent", QVariant::fromValue(window->contentItem()));
    dialog->setProperty("nameFilters", filtersForMimeType(mimeType));
    dialog->setProperty("fileName", suggestedName);

    m_pending = data;
    m_accepted = false;
    m_dialog = dialog;

    connect(dialog, SIGNAL(fileAccepted(QString)), this, SLOT(onFileAccepted(QString)));
    connect(dialog, SIGNAL(closed()), this, SLOT(onDialogClosed()));

    QMetaObject::invokeMethod(dialog, "open");
}

void FileSaverQt::onFileAccepted(const QString& path)
{
    m_accepted = true;
    write(path);
}

void FileSaverQt::onDialogClosed()
{
    if (!m_accepted && m_onCancelled)
        m_onCancelled();

    if (m_dialog)
        m_dialog->deleteLater();
    m_dialog = nullptr;
    m_accepted = false;
    m_pending.clear();
}

void FileSaverQt::write(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        if (m_onFailed)
            m_onFailed(file.errorString());
        return;
    }
    if (file.write(m_pending) != m_pending.size())
    {
        if (m_onFailed)
            m_onFailed(file.errorString());
        return;
    }
    file.close();
    if (m_onSaved)
        m_onSaved(path);
}
