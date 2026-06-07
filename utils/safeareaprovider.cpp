#include "safeareaprovider.h"

#ifdef Q_OS_ANDROID

#include <QGuiApplication>
#include <QJniObject>
#include <QScreen>

void SafeAreaProvider::refresh()
{
    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    if (!activity.isValid())
    {
        return;
    }

    QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
    QJniObject decor = window.isValid()
        ? window.callObjectMethod("getDecorView", "()Landroid/view/View;")
        : QJniObject();
    QJniObject insets = decor.isValid()
        ? decor.callObjectMethod("getRootWindowInsets", "()Landroid/view/WindowInsets;")
        : QJniObject();
    if (!insets.isValid())
    {
        return;
    }

    int top = 0, bottom = 0, left = 0, right = 0;
    const int sdk = QJniObject::getStaticField<jint>("android/os/Build$VERSION", "SDK_INT");
    if (sdk >= 30)
    {
        const int systemBars = QJniObject::callStaticMethod<jint>("android/view/WindowInsets$Type",
                                                                  "systemBars", "()I");
        const int displayCutout = QJniObject::callStaticMethod<jint>(
            "android/view/WindowInsets$Type", "displayCutout", "()I");

        QJniObject in = insets.callObjectMethod("getInsets", "(I)Landroid/graphics/Insets;",
                                                systemBars | displayCutout);
        if (in.isValid())
        {
            top = in.getField<jint>("top");
            bottom = in.getField<jint>("bottom");
            left = in.getField<jint>("left");
            right = in.getField<jint>("right");
        }
    }
    else
    {
        top = insets.callMethod<jint>("getSystemWindowInsetTop", "()I");
        bottom = insets.callMethod<jint>("getSystemWindowInsetBottom", "()I");
        left = insets.callMethod<jint>("getSystemWindowInsetLeft", "()I");
        right = insets.callMethod<jint>("getSystemWindowInsetRight", "()I");
    }

    QScreen* screen = QGuiApplication::primaryScreen();
    const qreal dpr = screen ? screen->devicePixelRatio() : 1.0;
    setInsets(qRound(top / dpr), qRound(bottom / dpr), qRound(left / dpr), qRound(right / dpr));
}

#else

void SafeAreaProvider::refresh() { }

#endif

SafeAreaProvider::SafeAreaProvider(QObject* parent)
    : QObject(parent)
{
    refresh();
}

void SafeAreaProvider::setInsets(int top, int bottom, int left, int right)
{
    if (m_top == top && m_bottom == bottom && m_left == left && m_right == right)
    {
        return;
    }

    m_top = top;
    m_bottom = bottom;
    m_left = left;
    m_right = right;
    emit insetsChanged();
}
