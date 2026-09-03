#include "notification.h"
#include "context.h"

namespace android
{

Notification::Builder::Builder(const Context& context, const QString& channelId)
    : m_builder("android/app/Notification$Builder",
                "(Landroid/content/Context;Ljava/lang/String;)V",
                context.jniObject().object<jobject>(),
                QJniObject::fromString(channelId).object<jstring>())
{
}

Notification::Builder& Notification::Builder::setContentTitle(const QString& title)
{
    m_builder.callObjectMethod("setContentTitle",
                               "(Ljava/lang/CharSequence;)Landroid/app/Notification$Builder;",
                               QJniObject::fromString(title).object<jstring>());
    return *this;
}

Notification::Builder& Notification::Builder::setContentText(const QString& text)
{
    m_builder.callObjectMethod("setContentText",
                               "(Ljava/lang/CharSequence;)Landroid/app/Notification$Builder;",
                               QJniObject::fromString(text).object<jstring>());
    return *this;
}

Notification::Builder& Notification::Builder::setSmallIcon(int resourceId)
{
    m_builder.callObjectMethod("setSmallIcon", "(I)Landroid/app/Notification$Builder;",
                               static_cast<jint>(resourceId));
    return *this;
}

Notification::Builder& Notification::Builder::setAutoCancel(bool autoCancel)
{
    m_builder.callObjectMethod("setAutoCancel", "(Z)Landroid/app/Notification$Builder;",
                               static_cast<jboolean>(autoCancel));
    return *this;
}

Notification Notification::Builder::build() const
{
    return Notification(m_builder.callObjectMethod("build", "()Landroid/app/Notification;"));
}

Notification::Notification(QJniObject jni)
    : m_notification(std::move(jni))
{
}

QJniObject Notification::jniObject() const { return m_notification; }

}  // namespace android
