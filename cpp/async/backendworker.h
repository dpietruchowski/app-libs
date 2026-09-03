#pragma once

#include <QObject>
#include <QThread>

class BackendWorker final : public QObject
{
public:
    BackendWorker();
    ~BackendWorker() override;

private:
    QThread m_thread;
};
