#pragma once

#include <functional>
#include <QFileSystemWatcher>
#include <QObject>
#include <QDir>
#include <QTimer>
#include <QString>

class FileWatcher : public QObject
{
    Q_OBJECT
public:
    explicit FileWatcher(std::function<void()> callback, QObject *parent = nullptr);

    void setDirectory(const QString &path);

public slots:
    void directoryChanged(const QString &path);
    void fileChanged(const QString &path);

private:
    void addPaths();

private:
    std::function<void()> mCallback;
    QFileSystemWatcher mWatcher;
    QDir mDir;
    QTimer mTimer;
};
