#include <QDebug>
#include <QString>
#include "filewatcher.h"

FileWatcher::FileWatcher(std::function<void()> callback, QObject *parent)
    : QObject(parent)
    , mCallback(callback)
{
    QObject::connect(&mWatcher, &QFileSystemWatcher::directoryChanged, this, &FileWatcher::directoryChanged);
    QObject::connect(&mWatcher, &QFileSystemWatcher::fileChanged, this, &FileWatcher::fileChanged);

    mTimer.setInterval(100);
    mTimer.setSingleShot(true);

    mDir.setFilter(QDir::Files | QDir::NoSymLinks);
    mDir.setNameFilters(QStringList() << "*.qml");

    QObject::connect(&mTimer, &QTimer::timeout, mCallback);
}

void FileWatcher::addPaths()
{
    QStringList entries = mDir.entryList();
    for (int i = 0; i < entries.size(); i++)
    {
        entries[i].prepend(mDir.absolutePath() + "/");
    }

    mWatcher.addPaths(entries);
    mTimer.start();
}

void FileWatcher::setDirectory(const QString &path)
{
    if (!mWatcher.addPath(path))
    {
        qWarning() << "Could not add path:" << path;
    }
    else
    {
        mDir.setPath(path);
        addPaths();
    }
}

void FileWatcher::directoryChanged(const QString &path)
{
    Q_UNUSED(path);
    addPaths();
}

void FileWatcher::fileChanged(const QString &path)
{
    Q_UNUSED(path);
    mTimer.start();
}
