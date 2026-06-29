#include "keyboardinsetprovider.h"

#include <QGuiApplication>
#include <QScreen>

#ifdef Q_OS_ANDROID
#include <QInputMethod>
#include <QJniObject>
#include <jni.h>

namespace
{
KeyboardInsetProvider* g_instance = nullptr;

bool usesNativeImeInset()
{
    return QJniObject::getStaticField<jint>("android/os/Build$VERSION", "SDK_INT") >= 30;
}

int readImeInsetPx()
{
    auto future = QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() -> QVariant {
        QJniObject activity = QNativeInterface::QAndroidApplication::context();
        if (!activity.isValid())
        {
            return 0;
        }
        QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
        QJniObject decor = window.isValid()
            ? window.callObjectMethod("getDecorView", "()Landroid/view/View;")
            : QJniObject();
        if (!decor.isValid())
        {
            return 0;
        }
        QJniObject insets =
            decor.callObjectMethod("getRootWindowInsets", "()Landroid/view/WindowInsets;");
        if (!insets.isValid())
        {
            return 0;
        }
        const jint imeType =
            QJniObject::callStaticMethod<jint>("android/view/WindowInsets$Type", "ime", "()I");
        QJniObject imeInsets =
            insets.callObjectMethod("getInsets", "(I)Landroid/graphics/Insets;", imeType);
        if (!imeInsets.isValid())
        {
            return 0;
        }
        return imeInsets.getField<jint>("bottom");
    });
    future.waitForFinished();
    return future.resultCount() > 0 ? future.result().toInt() : 0;
}
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

    if (usesNativeImeInset())
    {
        QInputMethod* inputMethod = QGuiApplication::inputMethod();
        connect(inputMethod, &QInputMethod::visibleChanged, this,
                &KeyboardInsetProvider::refreshImeInset);
        connect(inputMethod, &QInputMethod::animatingChanged, this,
                &KeyboardInsetProvider::refreshImeInset);
        connect(inputMethod, &QInputMethod::keyboardRectangleChanged, this,
                &KeyboardInsetProvider::refreshImeInset);
    }
    else
    {
        QNativeInterface::QAndroidApplication::runOnAndroidMainThread([] {
            QJniObject activity = QNativeInterface::QAndroidApplication::context();
            QJniObject::callStaticMethod<void>("com/fillin/app/KeyboardHeightProvider", "install",
                                               "(Landroid/app/Activity;)V", activity.object());
            return QVariant();
        });
    }
#endif
}

KeyboardInsetProvider::~KeyboardInsetProvider()
{
#ifdef Q_OS_ANDROID
    g_instance = nullptr;
#endif
}

void KeyboardInsetProvider::refreshImeInset()
{
#ifdef Q_OS_ANDROID
    setBottomFromPx(readImeInsetPx());
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
