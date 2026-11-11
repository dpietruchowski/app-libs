#include "androidkeyguard.h"

#include "platform/keyguard.h"

#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>

namespace
{

const char* const lockScreenClass = "app/libs/LockScreen";

QBasicMutex activeOwnerMutex;
Keyguard* activeOwner = nullptr;

void setListening(bool listening)
{
    QJniObject::callStaticMethod<void>(lockScreenClass, "setListening", "(Z)V",
                                       static_cast<jboolean>(listening));
}

void refreshState(Keyguard& owner)
{
    const QJniObject context(QNativeInterface::QAndroidApplication::context());
    const bool locked = context.isValid()
        && QJniObject::callStaticMethod<jboolean>(lockScreenClass, "isLocked",
                                                  "(Landroid/content/Context;)Z", context.object());
    const bool shownOverLock
        = QJniObject::callStaticMethod<jboolean>(lockScreenClass, "isShownOverLock", "()Z");
    owner.update(locked, shownOverLock);
}

template <typename Handler> void postToOwner(Handler handler)
{
    const QMutexLocker locker(&activeOwnerMutex);
    if (activeOwner == nullptr)
        return;

    Keyguard* owner = activeOwner;
    QMetaObject::invokeMethod(owner, [owner, handler] { handler(*owner); }, Qt::QueuedConnection);
}

void onStateChanged(JNIEnv*, jclass)
{
    postToOwner([](Keyguard& owner) { refreshState(owner); });
}

void onUnlockFinished(JNIEnv*, jclass, jboolean succeeded)
{
    postToOwner(
        [succeeded](Keyguard& owner)
        {
            refreshState(owner);
            owner.finishUnlock(succeeded);
        });
}

void registerNatives()
{
    static const JNINativeMethod methods[] = {
        { "nativeStateChanged", "()V", reinterpret_cast<void*>(onStateChanged) },
        { "nativeUnlockFinished", "(Z)V", reinterpret_cast<void*>(onUnlockFinished) },
    };
    QJniEnvironment environment;
    environment.registerNativeMethods(lockScreenClass, methods, 2);
}

}  // namespace

KeyguardBackend::KeyguardBackend(Keyguard& owner)
    : m_owner(owner)
{
    registerNatives();
    {
        const QMutexLocker locker(&activeOwnerMutex);
        activeOwner = &m_owner;
    }
    setListening(true);
    refreshState(m_owner);
}

KeyguardBackend::~KeyguardBackend()
{
    setListening(false);
    const QMutexLocker locker(&activeOwnerMutex);
    activeOwner = nullptr;
}

void KeyguardBackend::requestUnlock()
{
    const QJniObject context(QNativeInterface::QAndroidApplication::context());
    if (!context.isValid())
    {
        m_owner.finishUnlock(false);
        return;
    }
    QJniObject::callStaticMethod<void>(lockScreenClass, "requestUnlock",
                                       "(Landroid/content/Context;)V", context.object());
}

void KeyguardBackend::returnToLockScreen()
{
    const QJniObject context(QNativeInterface::QAndroidApplication::context());
    if (!context.isValid())
        return;

    QJniObject::callStaticMethod<void>(lockScreenClass, "returnToLockScreen",
                                       "(Landroid/content/Context;)V", context.object());
}
