#include "alarmmanager.h"
#include "context.h"
#include "pendingintent.h"

namespace android
{

AlarmManager AlarmManager::instance()
{
    return AlarmManager(Context::application().systemService("alarm"));
}

AlarmManager::AlarmManager(QJniObject jni)
    : m_manager(std::move(jni))
{
}

void AlarmManager::setExactAndAllowWhileIdle(qint64 triggerAtMillis,
                                             const PendingIntent& operation) const
{
    m_manager.callMethod<void>("setExactAndAllowWhileIdle", "(IJLandroid/app/PendingIntent;)V",
                               static_cast<jint>(0), static_cast<jlong>(triggerAtMillis),
                               operation.jniObject().object<jobject>());
}

void AlarmManager::setAndAllowWhileIdle(qint64 triggerAtMillis,
                                        const PendingIntent& operation) const
{
    m_manager.callMethod<void>("setAndAllowWhileIdle", "(IJLandroid/app/PendingIntent;)V",
                               static_cast<jint>(0), static_cast<jlong>(triggerAtMillis),
                               operation.jniObject().object<jobject>());
}

void AlarmManager::cancel(const PendingIntent& operation) const
{
    m_manager.callMethod<void>("cancel", "(Landroid/app/PendingIntent;)V",
                               operation.jniObject().object<jobject>());
}

}  // namespace android
