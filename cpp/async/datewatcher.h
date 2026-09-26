#pragma once

#include <QDate>
#include <QObject>
#include <QTimer>

class DateWatcher final : public QObject
{
    Q_OBJECT

public:
    static DateWatcher& instance();

    void start();
    void check();

signals:
    void dateChanged(const QDate& date);

private:
    DateWatcher();

    void scheduleNextMidnight();

    QDate m_date;
    QTimer m_midnightTimer;
};
