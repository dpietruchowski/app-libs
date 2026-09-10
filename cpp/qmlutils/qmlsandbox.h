#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

class QQmlEngine;
class QFileSystemWatcher;
class QTimer;

class QmlSandbox : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active CONSTANT)
    Q_PROPERTY(QString directory READ directory CONSTANT)
    Q_PROPERTY(QStringList files READ files NOTIFY filesChanged)
    Q_PROPERTY(QString currentFile READ currentFile WRITE setCurrentFile NOTIFY currentFileChanged)
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

public:
    QmlSandbox(QQmlEngine& engine, const QString& directory, const QStringList& watchedDirs,
               QObject* parent = nullptr);

    bool active() const;
    QString directory() const;
    QStringList files() const;
    QString currentFile() const;
    void setCurrentFile(const QString& file);
    int revision() const;

    Q_INVOKABLE QUrl urlFor(const QString& file) const;
    Q_INVOKABLE void reload();

signals:
    void filesChanged();
    void currentFileChanged();
    void revisionChanged();

private:
    void rescanFiles();
    void watchTree(const QString& root);
    void watchAllTrees();

    QQmlEngine& m_engine;
    QString m_directory;
    QStringList m_watchRoots;
    QStringList m_files;
    QString m_currentFile;
    int m_revision = 0;
    QFileSystemWatcher* m_watcher = nullptr;
    QTimer* m_debounce = nullptr;
};
