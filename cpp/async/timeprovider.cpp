#include "timeprovider.h"

std::unique_ptr<TimeProvider> TimeProvider::s_instance = nullptr;

TimeProvider& TimeProvider::instance()
{
    if (!s_instance)
    {
        s_instance = std::make_unique<SystemTimeProvider>();
    }
    return *s_instance;
}

void TimeProvider::setInstance(std::unique_ptr<TimeProvider> provider)
{
    s_instance = std::move(provider);
}

QDateTime SystemTimeProvider::currentDateTime() const { return QDateTime::currentDateTime(); }

QDate SystemTimeProvider::currentDate() const { return QDate::currentDate(); }
