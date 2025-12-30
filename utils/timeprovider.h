#pragma once

#include <QDate>
#include <QDateTime>
#include <memory>

class TimeProvider
{
public:
    static TimeProvider& instance();
    static void setInstance(std::unique_ptr<TimeProvider> provider);

    virtual ~TimeProvider() = default;

    virtual QDateTime currentDateTime() const = 0;
    virtual QDate currentDate() const = 0;

protected:
    TimeProvider() = default;

private:
    static std::unique_ptr<TimeProvider> s_instance;
};

class SystemTimeProvider final : public TimeProvider
{
public:
    QDateTime currentDateTime() const override;
    QDate currentDate() const override;
};
