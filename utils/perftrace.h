#pragma once

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QString>
#include <QThread>

namespace perftrace
{
inline const char* threadLabel()
{
    QCoreApplication* app = QCoreApplication::instance();
    if (app && QThread::currentThread() == app->thread())
        return "GUI";
    return "worker";
}

class Scope final
{
public:
    explicit Scope(QString label)
        : m_label(std::move(label))
    {
        m_timer.start();
    }

    void lap(const char* stage)
    {
        const qint64 now = m_timer.nsecsElapsed();
        qInfo("PERF [%-6s] %-34s %-22s %8.2f ms (od startu %8.2f ms)", threadLabel(),
              qPrintable(m_label), stage, (now - m_last) / 1e6, now / 1e6);
        m_last = now;
    }

    qreal elapsedMs() const { return m_timer.nsecsElapsed() / 1e6; }

private:
    QString m_label;
    QElapsedTimer m_timer;
    qint64 m_last = 0;
};
}
