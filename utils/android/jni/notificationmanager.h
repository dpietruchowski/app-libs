#pragma once

#include <QJniObject>
#include <QString>

namespace android
{

class Notification;
class NotificationChannel;

class NotificationManager final
{
public:
    static NotificationManager instance();

    void createChannel(const NotificationChannel& channel) const;
    void notify(int id, const Notification& notification) const;
    void cancel(int id) const;

private:
    explicit NotificationManager(QJniObject jni);

    QJniObject m_manager;
};

}  // namespace android
