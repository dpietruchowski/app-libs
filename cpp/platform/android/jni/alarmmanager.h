#pragma once

#include <QJniObject>

namespace android
{

class PendingIntent;

class AlarmManager final
{
public:
    static AlarmManager instance();

    void setExactAndAllowWhileIdle(qint64 triggerAtMillis, const PendingIntent& operation) const;

private:
    explicit AlarmManager(QJniObject jni);

    QJniObject m_manager;
};

}  // namespace android
