#include "keyboardinsetprovider.h"

#include <QGuiApplication>
#include <QScreen>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <jni.h>

namespace
{
KeyboardInsetProvider* g_instance = nullptr;
}

extern "C" JNIEXPORT void JNICALL Java_com_fillin_app_KeyboardHeightProvider_onKeyboardHeight(
    JNIEnv*, jclass, jint px)
{
    KeyboardInsetProvider* instance = g_instance;
    if (!instance)
    {
        return;
    }
    QMetaObject::invokeMethod(
        instance, [instance, px] { instance->setBottomFromPx(px); }, Qt::QueuedConnection);
}
#endif

KeyboardInsetProvider::KeyboardInsetProvider(QObject* parent)
    : QObject(parent)
{
#ifdef Q_OS_ANDROID
    g_instance = this;

    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([] {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        QJniObject::callStaticMethod<void>("com/fillin/app/KeyboardHeightProvider", "install",
                                           "(Landroid/app/Activity;)V", activity.object());
        return QVariant();
    });
#endif
}

KeyboardInsetProvider::~KeyboardInsetProvider()
{
#ifdef Q_OS_ANDROID
    g_instance = nullptr;
#endif
}

void KeyboardInsetProvider::setBottomFromPx(int px)
{
    QScreen* screen = QGuiApplication::primaryScreen();
    const qreal dpr = screen ? screen->devicePixelRatio() : 1.0;
    setBottom(qRound(px / dpr));
}

void KeyboardInsetProvider::setBottom(int bottom)
{
    if (m_bottom == bottom)
    {
        return;
    }

    m_bottom = bottom;
    emit bottomChanged();
}
