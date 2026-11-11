#include "androidquickentry.h"

#include "platform/quickentry.h"

#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>

namespace
{

const char* const quickEntryClass = "app/libs/QuickEntry";

QBasicMutex activeOwnerMutex;
QuickEntry* activeOwner = nullptr;

QJniObject applicationContext()
{
    return QJniObject(QNativeInterface::QAndroidApplication::context());
}

QList<QuickEntryAction> pendingActions()
{
    const QJniObject context = applicationContext();
    if (!context.isValid())
        return {};

    const QString json = QJniObject::callStaticObjectMethod(
                             quickEntryClass, "pending",
                             "(Landroid/content/Context;)Ljava/lang/String;", context.object())
                             .toString();
    return QuickEntryAction::listFromJson(json.toUtf8());
}

void onActionsAvailable(JNIEnv*, jclass)
{
    const QMutexLocker locker(&activeOwnerMutex);
    if (activeOwner == nullptr)
        return;

    QuickEntry* owner = activeOwner;
    QMetaObject::invokeMethod(
        owner, [owner] { owner->deliver(pendingActions()); }, Qt::QueuedConnection);
}

void registerNatives()
{
    static const JNINativeMethod methods[] = {
        { "nativeActionsAvailable", "()V", reinterpret_cast<void*>(onActionsAvailable) },
    };
    QJniEnvironment environment;
    environment.registerNativeMethods(quickEntryClass, methods, 1);
}

void setListening(bool listening)
{
    QJniObject::callStaticMethod<void>(quickEntryClass, "setListening", "(Z)V",
                                       static_cast<jboolean>(listening));
}

void callWithContext(const char* method)
{
    const QJniObject context = applicationContext();
    if (context.isValid())
        QJniObject::callStaticMethod<void>(quickEntryClass, method, "(Landroid/content/Context;)V",
                                           context.object());
}

void callWithContextAndString(const char* method, const QString& text)
{
    const QJniObject context = applicationContext();
    if (!context.isValid())
        return;

    const QJniObject javaText = QJniObject::fromString(text);
    QJniObject::callStaticMethod<void>(quickEntryClass, method,
                                       "(Landroid/content/Context;Ljava/lang/String;)V",
                                       context.object(), javaText.object<jstring>());
}

}  // namespace

QuickEntryBackend::QuickEntryBackend(QuickEntry& owner)
    : m_owner(owner)
{
    registerNatives();
    const QMutexLocker locker(&activeOwnerMutex);
    activeOwner = &m_owner;
}

QuickEntryBackend::~QuickEntryBackend()
{
    setListening(false);
    const QMutexLocker locker(&activeOwnerMutex);
    activeOwner = nullptr;
}

void QuickEntryBackend::configure(const QByteArray& json)
{
    callWithContextAndString("configure", QString::fromUtf8(json));
}

bool QuickEntryBackend::canShow() const
{
    const QJniObject context = applicationContext();
    return context.isValid()
        && QJniObject::callStaticMethod<jboolean>(quickEntryClass, "canShow",
                                                  "(Landroid/content/Context;)Z", context.object());
}

bool QuickEntryBackend::show()
{
    callWithContext("show");
    return true;
}

void QuickEntryBackend::hide() { callWithContext("hide"); }

void QuickEntryBackend::setHeader(const QString& header)
{
    callWithContextAndString("setHeader", header);
}

void QuickEntryBackend::startDelivery()
{
    setListening(true);
    m_owner.deliver(pendingActions());
}

void QuickEntryBackend::acknowledge(const QString& id)
{
    callWithContextAndString("acknowledge", id);
}
