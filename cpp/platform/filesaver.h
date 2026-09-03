#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include <memory>

class QQmlEngine;

class FileSaver final : public QObject
{
    Q_OBJECT

public:
    explicit FileSaver(QObject* parent = nullptr);
    ~FileSaver() override;

    static void setQmlEngine(QQmlEngine* engine);

    void save(const QString& suggestedName, const QString& mimeType, const QByteArray& data);

signals:
    void saved(const QString& location);
    void cancelled();
    void failed(const QString& message);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
