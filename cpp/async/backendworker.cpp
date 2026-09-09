#include "async/backendworker.h"

#include <QEventLoop>

BackendWorker::BackendWorker()
{
    moveToThread(&m_thread);
    m_thread.start();
}

BackendWorker::~BackendWorker()
{
    m_thread.quit();
    m_thread.wait();
}

void BackendWorker::drain(int maxRounds)
{
    QEventLoop loop;
    for (int i = 0; i < maxRounds; ++i)
    {
        QMetaObject::invokeMethod(this, [] {}, Qt::BlockingQueuedConnection);
        if (!loop.processEvents(QEventLoop::AllEvents))
            break;
    }
}
