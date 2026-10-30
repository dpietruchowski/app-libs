#pragma once

#include <QObject>
#include <QThread>

class BackendWorker final : public QObject
{
public:
    BackendWorker();
    ~BackendWorker() override;

    void drain(int maxRounds = 64);

private:
    QThread m_thread;
};
