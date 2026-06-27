#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include <memory>

class QQmlEngine;

class FileOpener final : public QObject
{
    Q_OBJECT

public:
    explicit FileOpener(QObject* parent = nullptr);
    ~FileOpener() override;

    static void setQmlEngine(QQmlEngine* engine);

    void open(const QString& mimeType = QString());

signals:
    void opened(const QString& location, const QByteArray& data);
    void cancelled();
    void failed(const QString& message);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};