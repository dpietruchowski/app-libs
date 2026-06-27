#include "fileopenerqt.h"

#include <QFile>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QWindow>

QQmlEngine* FileOpenerQt::s_engine = nullptr;

namespace
{
QStringList filtersForMimeType(const QString& mimeType)
{
    if (mimeType == QStringLiteral("application/json"))
        return { QStringLiteral("*.json") };
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

void FileOpenerQt::setQmlEngine(QQmlEngine* engine) { s_engine = engine; }

FileOpenerQt::FileOpenerQt(OpenedCallback onOpened, CancelledCallback onCancelled,
                           FailedCallback onFailed)
    : m_onOpened(std::move(onOpened))
    , m_onCancelled(std::move(onCancelled))
    , m_onFailed(std::move(onFailed))
{
}

void FileOpenerQt::launch(const QString& mimeType, qint64 maxBytes)
{
    if (m_dialog)
        return;

    m_maxBytes = maxBytes;

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
    component.setData("import Themed.Components\nThemedFileDialog {}", QUrl());
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

    m_selected = false;
    m_dialog = dialog;

    connect(dialog, SIGNAL(fileSelected(QString)), this, SLOT(onFileSelected(QString)));
    connect(dialog, SIGNAL(closed()), this, SLOT(onDialogClosed()));

    QMetaObject::invokeMethod(dialog, "open");
}

void FileOpenerQt::onFileSelected(const QString& path)
{
    m_selected = true;
    read(path);
}

void FileOpenerQt::onDialogClosed()
{
    if (!m_selected && m_onCancelled)
        m_onCancelled();

    if (m_dialog)
        m_dialog->deleteLater();
    m_dialog = nullptr;
    m_selected = false;
}

void FileOpenerQt::read(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (m_onFailed)
            m_onFailed(file.errorString());
        return;
    }
    if (m_maxBytes > 0 && file.size() > m_maxBytes)
    {
        file.close();
        if (m_onFailed)
            m_onFailed(QStringLiteral("File is too large (limit %1 MB)")
                           .arg(m_maxBytes / (1024 * 1024)));
        return;
    }
    const QByteArray data = file.readAll();
    file.close();
    if (m_onOpened)
        m_onOpened(path, data);
}
