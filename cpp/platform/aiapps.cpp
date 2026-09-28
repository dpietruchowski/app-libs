#include "aiapps.h"

#include <QBuffer>
#include <QByteArray>
#include <QDesktopServices>
#include <QImage>
#include <QUrl>
#include <QtGlobal>

#include <array>

#if defined(Q_OS_ANDROID)
#include "android/jni/androidpackage.h"
#endif

namespace
{

struct AiAppEntry final
{
    const char* id;
    const char* name;
    const char* webUrl;
};

constexpr std::array<AiAppEntry, 3> kAiApps { {
    { "com.openai.chatgpt", "ChatGPT", "https://chatgpt.com/" },
    { "com.google.android.apps.bard", "Gemini", "https://gemini.google.com/app" },
    { "com.anthropic.claude", "Claude", "https://claude.ai/new" },
} };

const AiAppEntry* find(const QString& id)
{
    for (const AiAppEntry& entry : kAiApps)
    {
        if (QString::fromLatin1(entry.id) == id)
        {
            return &entry;
        }
    }
    return nullptr;
}

#if defined(Q_OS_ANDROID)

constexpr int kIconMaxSize = 96;

QString encodeIconDataUri(const QImage& icon)
{
    if (icon.isNull())
    {
        return QString();
    }

    const QImage scaled = icon.width() > kIconMaxSize || icon.height() > kIconMaxSize
                              ? icon.scaled(kIconMaxSize, kIconMaxSize, Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation)
                              : icon;

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    scaled.save(&buffer, "PNG");
    return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(bytes.toBase64());
}

#endif

}  // namespace

namespace ai_apps
{

QVariantList catalog()
{
    QVariantList apps;
    for (const AiAppEntry& entry : kAiApps)
    {
        const QString id = QString::fromLatin1(entry.id);
        bool installed = false;
        QString iconSource;

#if defined(Q_OS_ANDROID)
        const android::AndroidPackage package(id);
        installed = package.isInstalled();
        if (installed)
        {
            iconSource = encodeIconDataUri(package.loadIcon());
        }
#endif

        QVariantMap app;
        app["id"] = id;
        app["name"] = QString::fromLatin1(entry.name);
        app["installed"] = installed;
        app["iconSource"] = iconSource;
        apps.append(app);
    }
    return apps;
}

QStringList names()
{
    QStringList result;
    for (const AiAppEntry& entry : kAiApps)
    {
        result.append(QString::fromLatin1(entry.name));
    }
    return result;
}

QString idAt(int index)
{
    if (index < 0 || index >= static_cast<int>(kAiApps.size()))
    {
        return QString();
    }
    return QString::fromLatin1(kAiApps[static_cast<size_t>(index)].id);
}

int indexOf(const QString& id)
{
    const AiAppEntry* entry = find(id);
    if (entry == nullptr)
    {
        return -1;
    }
    return static_cast<int>(entry - kAiApps.data());
}

void open(const QString& id)
{
    const AiAppEntry* entry = find(id);
    if (entry == nullptr)
    {
        return;
    }

#if defined(Q_OS_ANDROID)
    const android::AndroidPackage package(id);
    if (package.isInstalled())
    {
        package.launch();
        return;
    }
#endif

    QDesktopServices::openUrl(QUrl(QString::fromLatin1(entry->webUrl)));
}

}  // namespace ai_apps
