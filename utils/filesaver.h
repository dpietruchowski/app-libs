#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include <memory>

class FileSaver final : public QObject
{
    Q_OBJECT

public:
    explicit FileSaver(QObject* parent = nullptr);
    ~FileSaver() override;

    void save(const QString& suggestedName, const QString& mimeType, const QByteArray& data);

signals:
    void saved(const QString& location);
    void cancelled();
    void failed(const QString& message);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};
