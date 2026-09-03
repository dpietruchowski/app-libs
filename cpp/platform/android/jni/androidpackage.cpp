#include "androidpackage.h"

#include <QCoreApplication>
#include <QJniEnvironment>
#include <QJniObject>

#include <utility>

namespace
{

QJniObject packageManager()
{
    QJniObject context(QNativeInterface::QAndroidApplication::context());
    return context.callObjectMethod("getPackageManager",
                                    "()Landroid/content/pm/PackageManager;");
}

QJniObject launchIntentFor(const QString& package)
{
    QJniObject pm = packageManager();
    if (!pm.isValid())
        return { };

    return pm.callObjectMethod("getLaunchIntentForPackage",
                               "(Ljava/lang/String;)Landroid/content/Intent;",
                               QJniObject::fromString(package).object<jstring>());
}

}  // namespace

namespace android
{

AndroidPackage::AndroidPackage(QString package)
    : m_package(std::move(package))
{
}

bool AndroidPackage::isInstalled() const { return launchIntentFor(m_package).isValid(); }

void AndroidPackage::launch() const
{
    QJniObject intent = launchIntentFor(m_package);
    if (!intent.isValid())
        return;

    QJniObject context(QNativeInterface::QAndroidApplication::context());
    context.callMethod<void>("startActivity", "(Landroid/content/Intent;)V", intent.object<jobject>());
}

QImage AndroidPackage::loadIcon() const
{
    QJniObject pm = packageManager();
    if (!pm.isValid())
        return { };

    QJniEnvironment env;
    QJniObject drawable
        = pm.callObjectMethod("getApplicationIcon",
                              "(Ljava/lang/String;)Landroid/graphics/drawable/Drawable;",
                              QJniObject::fromString(m_package).object<jstring>());
    if (env.checkAndClearExceptions() || !drawable.isValid())
        return { };

    int width = drawable.callMethod<jint>("getIntrinsicWidth");
    int height = drawable.callMethod<jint>("getIntrinsicHeight");
    if (width <= 0)
        width = 96;
    if (height <= 0)
        height = 96;

    QJniObject config = QJniObject::getStaticObjectField(
        "android/graphics/Bitmap$Config", "ARGB_8888", "Landroid/graphics/Bitmap$Config;");
    QJniObject bitmap = QJniObject::callStaticObjectMethod(
        "android/graphics/Bitmap", "createBitmap",
        "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;", width, height,
        config.object<jobject>());
    if (!bitmap.isValid())
        return { };

    QJniObject canvas("android/graphics/Canvas", "(Landroid/graphics/Bitmap;)V",
                      bitmap.object<jobject>());
    drawable.callMethod<void>("setBounds", "(IIII)V", 0, 0, width, height);
    drawable.callMethod<void>("draw", "(Landroid/graphics/Canvas;)V", canvas.object<jobject>());

    const int pixelCount = width * height;
    jintArray pixels = env->NewIntArray(pixelCount);
    bitmap.callMethod<void>("getPixels", "([IIIIIII)V", pixels, 0, width, 0, 0, width, height);

    QImage image(width, height, QImage::Format_ARGB32);
    env->GetIntArrayRegion(pixels, 0, pixelCount, reinterpret_cast<jint*>(image.bits()));
    env->DeleteLocalRef(pixels);

    return image;
}

}  // namespace android
