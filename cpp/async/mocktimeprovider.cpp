#include "mocktimeprovider.h"

#include "datewatcher.h"

void MockTimeProvider::setCurrentDateTime(const QDateTime& dateTime)
{
    m_currentDateTime = dateTime;
    notifyDateWatcher();
}

void MockTimeProvider::setCurrentDate(const QDate& date)
{
    m_currentDateTime = QDateTime(date, m_currentDateTime.time());
    notifyDateWatcher();
}

void MockTimeProvider::advanceSeconds(int seconds)
{
    if (m_currentDateTime.isValid())
    {
        m_currentDateTime = m_currentDateTime.addSecs(seconds);
    }
    else
    {
        m_currentDateTime = QDateTime::currentDateTime().addSecs(seconds);
    }
    notifyDateWatcher();
}

void MockTimeProvider::advanceDays(int days)
{
    if (m_currentDateTime.isValid())
    {
        m_currentDateTime = m_currentDateTime.addDays(days);
    }
    else
    {
        m_currentDateTime = QDateTime::currentDateTime().addDays(days);
    }
    notifyDateWatcher();
}

void MockTimeProvider::advanceDate(const QDate& targetDate)
{
    if (m_currentDateTime.isValid())
    {
        QDate current = m_currentDateTime.date();
        int days = current.daysTo(targetDate);
        m_currentDateTime = m_currentDateTime.addDays(days);
    }
    else
    {
        m_currentDateTime = QDateTime(targetDate, QTime(12, 0, 0));
    }
    notifyDateWatcher();
}

QDateTime MockTimeProvider::currentDateTime() const
{
    return m_currentDateTime.isValid() ? m_currentDateTime : QDateTime::currentDateTime();
}

QDate MockTimeProvider::currentDate() const
{
    return m_currentDateTime.isValid() ? m_currentDateTime.date() : QDate::currentDate();
}

void MockTimeProvider::notifyDateWatcher() const
{
    if (&TimeProvider::instance() == this)
        DateWatcher::instance().check();
}
