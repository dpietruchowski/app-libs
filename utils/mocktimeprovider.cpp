#include "mocktimeprovider.h"

void MockTimeProvider::setCurrentDateTime(const QDateTime& dateTime)
{
    m_currentDateTime = dateTime;
}

void MockTimeProvider::setCurrentDate(const QDate& date) { m_currentDate = date; }

QDateTime MockTimeProvider::currentDateTime() const
{
    return m_currentDateTime.isValid() ? m_currentDateTime : QDateTime::currentDateTime();
}

QDate MockTimeProvider::currentDate() const
{
    return m_currentDate.isValid() ? m_currentDate : QDate::currentDate();
}
