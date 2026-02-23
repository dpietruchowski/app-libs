#pragma once

#include <QJniObject>

namespace android
{

class Intent;

class PendingIntent final
{
public:
    static PendingIntent getBroadcast(int requestCode, const Intent& intent);

    QJniObject jniObject() const;

private:
    explicit PendingIntent(QJniObject jni);

    QJniObject m_pendingIntent;
};

}  // namespace android
