#pragma once

#include <QDateTime>
#include <QFile>
#include <QObject>
#include <QStandardPaths>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QJniEnvironment>
#endif

class FillInDbStorage;

class DatabaseExporter : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseExporter(FillInDbStorage* storage, QObject* parent = nullptr);

    Q_INVOKABLE void exportDatabase();

signals:
    void exportSuccess(const QString& path);
    void exportFailed(const QString& reason);

private:
    FillInDbStorage* m_storage = nullptr;
    QString m_dbPath;

#ifdef Q_OS_ANDROID
    bool exportViaMediaStore();
#endif
};
