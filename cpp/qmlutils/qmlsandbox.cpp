#include "qmlsandbox.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QQmlEngine>
#include <QTimer>
#include <utility>

namespace
{
constexpr int kReloadDebounceMs = 150;

QStringList qmlFilesIn(const QString& directory)
{
    return QDir(directory).entryList(QStringList { QStringLiteral("*.qml") }, QDir::Files,
                                     QDir::Name);
}

bool isQmlSingleton(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return false;
    }
    return file.readLine(64).startsWith("pragma Singleton");
}
}

QmlSandbox::QmlSandbox(QQmlEngine& engine, const QString& directory, const QStringList& watchedDirs,
                       QObject* parent)
    : QObject(parent)
    , m_engine(engine)
{
    if (directory.isEmpty() || !QDir(directory).exists())
    {
        return;
    }

    m_directory = QDir(directory).absolutePath();
    m_watchRoots.append(m_directory);
    for (const QString& dir : watchedDirs)
    {
        if (QDir(dir).exists())
        {
            m_watchRoots.append(QDir(dir).absolutePath());
        }
    }

    rescanFiles();

    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(kReloadDebounceMs);
    connect(m_debounce, &QTimer::timeout, this, &QmlSandbox::reload);

    m_watcher = new QFileSystemWatcher(this);
    watchAllTrees();
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this,
            [this](const QString& path)
            {
                m_singletonChanged = m_singletonChanged || isQmlSingleton(path);
                m_debounce->start();
            });
    connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
            [this] { m_debounce->start(); });

    qInfo() << "QML sandbox:" << m_directory << "-" << m_files.size() << "file(s), watching"
            << m_watchRoots.size() << "tree(s)";
}

bool QmlSandbox::active() const { return !m_directory.isEmpty(); }

QString QmlSandbox::directory() const { return m_directory; }

QStringList QmlSandbox::files() const { return m_files; }

QString QmlSandbox::currentFile() const { return m_currentFile; }

int QmlSandbox::revision() const { return m_revision; }

void QmlSandbox::setCurrentFile(const QString& file)
{
    if (file == m_currentFile || !m_files.contains(file))
    {
        return;
    }

    m_currentFile = file;
    emit currentFileChanged();
}

QUrl QmlSandbox::urlFor(const QString& file) const
{
    if (!active() || file.isEmpty())
    {
        return QUrl();
    }
    return QUrl::fromLocalFile(m_directory + QLatin1Char('/') + file);
}

void QmlSandbox::reload()
{
    if (!active())
    {
        return;
    }

    rescanFiles();
    watchAllTrees();
    m_engine.clearComponentCache();

    if (m_singletonChanged)
    {
        m_singletonChanged = false;
        qInfo() << "QML sandbox: singleton changed, restarting";
        QCoreApplication::exit(kRestartExitCode);
        return;
    }

    ++m_revision;
    emit revisionChanged();
}

void QmlSandbox::rescanFiles()
{
    const QStringList found = qmlFilesIn(m_directory);
    if (found != m_files)
    {
        m_files = found;
        emit filesChanged();
    }

    if (!m_files.contains(m_currentFile))
    {
        m_currentFile = m_files.value(0);
        emit currentFileChanged();
    }
}

void QmlSandbox::watchTree(const QString& root)
{
    const QStringList watchedDirs = m_watcher->directories();
    const QStringList watchedFiles = m_watcher->files();
    QStringList paths;

    const auto remember = [&](const QString& path, const QStringList& known)
    {
        if (!known.contains(path) && !paths.contains(path))
        {
            paths.append(path);
        }
    };

    remember(root, watchedDirs);

    QDirIterator it(root, QStringList { QStringLiteral("*.qml") }, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        const QString file = it.next();
        remember(file, watchedFiles);
        remember(QFileInfo(file).absolutePath(), watchedDirs);
    }

    if (!paths.isEmpty())
    {
        m_watcher->addPaths(paths);
    }
}

void QmlSandbox::watchAllTrees()
{
    for (const QString& root : std::as_const(m_watchRoots))
    {
        watchTree(root);
    }
}
