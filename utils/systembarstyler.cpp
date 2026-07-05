#include "systembarstyler.h"

#include <QEvent>
#include <QGuiApplication>
#include <QPalette>

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniObject>
#endif

SystemBarStyler::SystemBarStyler(QObject* parent)
    : QObject(parent)
{
    if (qGuiApp)
    {
        qGuiApp->installEventFilter(this);
    }
}

bool SystemBarStyler::systemDark() const
{
    return qGuiApp && qGuiApp->palette().color(QPalette::Window).lightnessF() < 0.5;
}

bool SystemBarStyler::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::ApplicationPaletteChange || event->type() == QEvent::ThemeChange)
    {
        emit systemDarkChanged();
    }
    return QObject::eventFilter(watched, event);
}

#ifdef Q_OS_ANDROID
namespace
{
void applyLightBarsOnMainThread(bool lightBars, jint backgroundArgb)
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread(
        [lightBars, backgroundArgb]() -> QVariant {
            QJniObject activity = QNativeInterface::QAndroidApplication::context();
            if (!activity.isValid())
            {
                return {};
            }
            QJniObject window = activity.callObjectMethod("getWindow", "()Landroid/view/Window;");
            if (!window.isValid())
            {
                return {};
            }

            const jint sdk =
                QJniObject::getStaticField<jint>("android/os/Build$VERSION", "SDK_INT");
            if (sdk >= 30)
            {
                QJniObject controller = window.callObjectMethod(
                    "getInsetsController", "()Landroid/view/WindowInsetsController;");
                if (controller.isValid())
                {
                    const jint mask =
                        QJniObject::getStaticField<jint>("android/view/WindowInsetsController",
                                                         "APPEARANCE_LIGHT_STATUS_BARS")
                        | QJniObject::getStaticField<jint>("android/view/WindowInsetsController",
                                                           "APPEARANCE_LIGHT_NAVIGATION_BARS");
                    controller.callMethod<void>("setSystemBarsAppearance", "(II)V",
                                                lightBars ? mask : 0, mask);
                }
            }
            else
            {
                QJniObject decor =
                    window.callObjectMethod("getDecorView", "()Landroid/view/View;");
                if (decor.isValid())
                {
                    const jint lightFlags =
                        QJniObject::getStaticField<jint>("android/view/View",
                                                         "SYSTEM_UI_FLAG_LIGHT_STATUS_BAR")
                        | QJniObject::getStaticField<jint>(
                            "android/view/View", "SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR");
                    jint visibility = decor.callMethod<jint>("getSystemUiVisibility", "()I");
                    visibility = lightBars ? (visibility | lightFlags)
                                           : (visibility & ~lightFlags);
                    decor.callMethod<void>("setSystemUiVisibility", "(I)V", visibility);
                }
            }

            if (sdk < 35)
            {
                const jint drawsBarBackgrounds = QJniObject::getStaticField<jint>(
                    "android/view/WindowManager$LayoutParams",
                    "FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS");
                window.callMethod<void>("addFlags", "(I)V", drawsBarBackgrounds);
                window.callMethod<void>("setStatusBarColor", "(I)V", backgroundArgb);
                window.callMethod<void>("setNavigationBarColor", "(I)V", backgroundArgb);
            }
            return {};
        });
}
}
#endif

void SystemBarStyler::apply(bool darkTheme, const QColor& background)
{
#ifdef Q_OS_ANDROID
    applyLightBarsOnMainThread(!darkTheme, static_cast<jint>(background.rgba()));
#else
    Q_UNUSED(darkTheme);
    Q_UNUSED(background);
#endif
}
