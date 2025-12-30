#pragma once

#include "timeprovider.h"
#include <QDate>
#include <QDateTime>

class MockTimeProvider final : public TimeProvider
{
public:
    void setCurrentDateTime(const QDateTime& dateTime);
    void setCurrentDate(const QDate& date);

    QDateTime currentDateTime() const override;
    QDate currentDate() const override;

private:
    QDateTime m_currentDateTime;
    QDate m_currentDate;
};
