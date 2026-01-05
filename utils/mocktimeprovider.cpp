#include "mocktimeprovider.h"

void MockTimeProvider::setCurrentDateTime(const QDateTime& dateTime)
{
    m_currentDateTime = dateTime;
}

void MockTimeProvider::setCurrentDate(const QDate& date)
{
    m_currentDateTime = QDateTime(date, m_currentDateTime.time());
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
}

QDateTime MockTimeProvider::currentDateTime() const
{
    return m_currentDateTime.isValid() ? m_currentDateTime : QDateTime::currentDateTime();
}

QDate MockTimeProvider::currentDate() const
{
    return m_currentDateTime.isValid() ? m_currentDateTime.date() : QDate::currentDate();
}
