#include "mocktimeprovider.h"

void MockTimeProvider::setCurrentDateTime(const QDateTime& dateTime)
{
    m_currentDateTime = dateTime;
}

void MockTimeProvider::setCurrentDate(const QDate& date) { m_currentDate = date; }

void MockTimeProvider::advanceDays(int days)
{
    QDate current = m_currentDate.isValid() ? m_currentDate : QDate::currentDate();
    m_currentDate = current.addDays(days);

    if (m_currentDateTime.isValid())
    {
        m_currentDateTime = m_currentDateTime.addDays(days);
    }
}

void MockTimeProvider::advanceDate(const QDate& targetDate)
{
    m_currentDate = targetDate;

    if (m_currentDateTime.isValid())
    {
        QDate current = m_currentDateTime.date();
        int days = current.daysTo(targetDate);
        m_currentDateTime = m_currentDateTime.addDays(days);
    }
}

QDateTime MockTimeProvider::currentDateTime() const
{
    return m_currentDateTime.isValid() ? m_currentDateTime : QDateTime::currentDateTime();
}

QDate MockTimeProvider::currentDate() const
{
    return m_currentDate.isValid() ? m_currentDate : QDate::currentDate();
}
