#include "androidlaunchaction.h"

#include "platform/launchaction.h"

#include <QCoreApplication>
#include <QJniObject>
#include <QMetaObject>

namespace
{

const char* const launchActionExtra = "launch_action";

QString takeLaunchAction(const QJniObject& intent)
{
    if (!intent.isValid())
        return {};

    const QJniObject key = QJniObject::fromString(QString::fromLatin1(launchActionExtra));
    const QString action
        = intent
              .callObjectMethod("getStringExtra", "(Ljava/lang/String;)Ljava/lang/String;",
                                key.object<jstring>())
              .toString();
    if (!action.isEmpty())
        intent.callMethod<void>("removeExtra", "(Ljava/lang/String;)V", key.object<jstring>());
    return action;
}

}  // namespace

LaunchActionListener::LaunchActionListener(LaunchAction& owner)
    : m_owner(owner)
{
    QtAndroidPrivate::registerNewIntentListener(this);

    const QJniObject activity(QNativeInterface::QAndroidApplication::context());
    m_owner.deliver(
        takeLaunchAction(activity.callObjectMethod("getIntent", "()Landroid/content/Intent;")));
}

LaunchActionListener::~LaunchActionListener()
{
    QtAndroidPrivate::unregisterNewIntentListener(this);
}

bool LaunchActionListener::handleNewIntent(JNIEnv*, jobject intent)
{
    const QString action = takeLaunchAction(QJniObject(intent));
    if (action.isEmpty())
        return false;

    LaunchAction* owner = &m_owner;
    QMetaObject::invokeMethod(
        owner, [owner, action] { owner->deliver(action); }, Qt::QueuedConnection);
    return true;
}
