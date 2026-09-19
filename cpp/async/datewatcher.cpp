#include "datewatcher.h"

#include <QDateTime>

#include "timeprovider.h"

DateWatcher& DateWatcher::instance()
{
    static DateWatcher watcher;
    return watcher;
}

DateWatcher::DateWatcher()
    : m_date(TimeProvider::instance().currentDate())
{
    m_midnightTimer.setSingleShot(true);
    m_midnightTimer.setTimerType(Qt::VeryCoarseTimer);
    connect(&m_midnightTimer, &QTimer::timeout, this,
            [this]
            {
                check();
                scheduleNextMidnight();
            });
}

void DateWatcher::start()
{
    m_date = TimeProvider::instance().currentDate();
    scheduleNextMidnight();
}

void DateWatcher::check()
{
    const QDate current = TimeProvider::instance().currentDate();
    if (current == m_date)
        return;

    m_date = current;
    emit dateChanged(current);
}

void DateWatcher::scheduleNextMidnight()
{
    const QDateTime now = QDateTime::currentDateTime();
    const QDateTime midnight(now.date().addDays(1), QTime(0, 0, 1));
    m_midnightTimer.start(std::chrono::milliseconds(now.msecsTo(midnight)));
}
