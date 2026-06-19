#include "utils/backendworker.h"

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
