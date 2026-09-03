#pragma once

#include <QJniObject>
#include <QString>

namespace android
{

class NotificationChannel final
{
public:
    explicit NotificationChannel(const QString& id, const QString& name, int importance);

    QJniObject jniObject() const;

private:
    QJniObject m_channel;
};

}  // namespace android
