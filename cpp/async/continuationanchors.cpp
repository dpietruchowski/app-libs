#include "async/continuationanchors.h"

#include <QCoreApplication>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QPointer>
#include <QThread>
#include <vector>

namespace
{
class ContinuationAnchors final
{
public:
    static ContinuationAnchors& instance()
    {
        static ContinuationAnchors anchors;
        return anchors;
    }

    static void destroyAll()
    {
        ContinuationAnchors& anchors = instance();
        QMutexLocker lock(&anchors.m_mutex);
        for (QObject* anchor : anchors.m_created)
        {
            delete anchor;
        }
        anchors.m_created.clear();
        anchors.m_byThread.clear();
    }

    QObject* anchorFor(QThread* thread)
    {
        QMutexLocker lock(&m_mutex);
        if (m_created.empty())
        {
            qAddPostRoutine(&ContinuationAnchors::destroyAll);
        }
        const auto found = m_byThread.constFind(thread);
        if (found != m_byThread.constEnd() && found->thread == thread)
        {
            return found->anchor;
        }
        auto* anchor = new QObject;
        if (thread != QThread::currentThread())
        {
            anchor->moveToThread(thread);
        }
        m_created.push_back(anchor);
        m_byThread.insert(thread, Entry { QPointer<QThread>(thread), anchor });
        return anchor;
    }

private:
    struct Entry
    {
        QPointer<QThread> thread;
        QObject* anchor = nullptr;
    };

    QMutex m_mutex;
    QHash<QThread*, Entry> m_byThread;
    std::vector<QObject*> m_created;
};
}

QObject* task_detail::continuationAnchorFor(QObject* context)
{
    return ContinuationAnchors::instance().anchorFor(context->thread());
}
