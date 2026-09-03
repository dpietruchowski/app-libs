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

}  // namespace android
