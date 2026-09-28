#include "pendingintent.h"
#include "context.h"
#include "intent.h"

namespace android
{

PendingIntent PendingIntent::getBroadcast(int requestCode, const Intent& intent)
{
    QJniObject context = Context::application().jniObject();
    // FLAG_IMMUTABLE = 0x04000000, FLAG_UPDATE_CURRENT = 0x08000000: a rescheduled alarm
    // must carry the new extras instead of the ones of the PendingIntent created first.
    QJniObject pi = QJniObject::callStaticObjectMethod(
        "android/app/PendingIntent", "getBroadcast",
        "(Landroid/content/Context;ILandroid/content/Intent;I)Landroid/app/PendingIntent;",
        context.object<jobject>(), static_cast<jint>(requestCode),
        intent.jniObject().object<jobject>(), static_cast<jint>(0x04000000 | 0x08000000));
    return PendingIntent(std::move(pi));
}

PendingIntent::PendingIntent(QJniObject jni)
    : m_pendingIntent(std::move(jni))
{
}

QJniObject PendingIntent::jniObject() const { return m_pendingIntent; }

}  // namespace android
