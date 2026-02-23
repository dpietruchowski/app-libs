#include "intent.h"

#include <QCoreApplication>

namespace
{

QString toString(android::Intent::Action action)
{
    switch (action)
    {
        case android::Intent::Action::View:
            return "android.intent.action.VIEW";
        case android::Intent::Action::Send:
            return "android.intent.action.SEND";
        case android::Intent::Action::Dial:
            return "android.intent.action.DIAL";
        case android::Intent::Action::Edit:
            return "android.intent.action.EDIT";
        case android::Intent::Action::Pick:
            return "android.intent.action.PICK";
        case android::Intent::Action::Search:
            return "android.intent.action.SEARCH";
        case android::Intent::Action::WebSearch:
            return "android.intent.action.WEB_SEARCH";
    }
    return {};
}

QString toString(android::Intent::Extra extra)
{
    switch (extra)
    {
        case android::Intent::Extra::Text:
            return "android.intent.extra.TEXT";
        case android::Intent::Extra::Subject:
            return "android.intent.extra.SUBJECT";
        case android::Intent::Extra::Email:
            return "android.intent.extra.EMAIL";
        case android::Intent::Extra::Stream:
            return "android.intent.extra.STREAM";
        case android::Intent::Extra::Title:
            return "android.intent.extra.TITLE";
    }
    return {};
}

QString toString(android::Intent::MimeType mimeType)
{
    switch (mimeType)
    {
        case android::Intent::MimeType::TextPlain:
            return "text/plain";
        case android::Intent::MimeType::TextHtml:
            return "text/html";
        case android::Intent::MimeType::ImageJpeg:
            return "image/jpeg";
        case android::Intent::MimeType::ImagePng:
            return "image/png";
        case android::Intent::MimeType::ImageAny:
            return "image/*";
        case android::Intent::MimeType::VideoAny:
            return "video/*";
        case android::Intent::MimeType::AudioAny:
            return "audio/*";
        case android::Intent::MimeType::ApplicationPdf:
            return "application/pdf";
    }
    return {};
}

}  // namespace

namespace android
{

Intent::Intent()
    : m_intent("android/content/Intent")
{
}

Intent::Intent(Action action)
    : m_intent("android/content/Intent")
{
    setAction(action);
}

Intent::Intent(const QString& customAction)
    : m_intent("android/content/Intent")
{
    m_intent.callObjectMethod("setAction", "(Ljava/lang/String;)Landroid/content/Intent;",
                              QJniObject::fromString(customAction).object<jstring>());
}

Intent::Intent(QJniObject jniObject)
    : m_intent(std::move(jniObject))
{
}

Intent& Intent::setAction(Action action)
{
    m_intent.callObjectMethod("setAction", "(Ljava/lang/String;)Landroid/content/Intent;",
                              QJniObject::fromString(toString(action)).object<jstring>());
    return *this;
}

Intent& Intent::setData(const QString& uri)
{
    QJniObject jUri = QJniObject::callStaticObjectMethod(
        "android/net/Uri", "parse", "(Ljava/lang/String;)Landroid/net/Uri;",
        QJniObject::fromString(uri).object<jstring>());
    m_intent.callObjectMethod("setData", "(Landroid/net/Uri;)Landroid/content/Intent;",
                              jUri.object<jobject>());
    return *this;
}

Intent& Intent::setClassName(const QString& packageName, const QString& className)
{
    m_intent.callObjectMethod(
        "setClassName",
        "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;",
        QJniObject::fromString(packageName).object<jstring>(),
        QJniObject::fromString(className).object<jstring>());
    return *this;
}

Intent& Intent::setType(MimeType mimeType)
{
    m_intent.callObjectMethod("setType", "(Ljava/lang/String;)Landroid/content/Intent;",
                              QJniObject::fromString(toString(mimeType)).object<jstring>());
    return *this;
}

Intent& Intent::putExtra(Extra key, const QString& value)
{
    m_intent.callObjectMethod("putExtra",
                              "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;",
                              QJniObject::fromString(toString(key)).object<jstring>(),
                              QJniObject::fromString(value).object<jstring>());
    return *this;
}

Intent& Intent::putExtra(const QString& key, const QString& value)
{
    m_intent.callObjectMethod("putExtra",
                              "(Ljava/lang/String;Ljava/lang/String;)Landroid/content/Intent;",
                              QJniObject::fromString(key).object<jstring>(),
                              QJniObject::fromString(value).object<jstring>());
    return *this;
}

Intent& Intent::putExtra(const QString& key, int value)
{
    m_intent.callObjectMethod("putExtra", "(Ljava/lang/String;I)Landroid/content/Intent;",
                              QJniObject::fromString(key).object<jstring>(),
                              static_cast<jint>(value));
    return *this;
}

Intent Intent::createChooser(const Intent& target, const QString& title)
{
    QJniObject chooser = QJniObject::callStaticObjectMethod(
        "android/content/Intent", "createChooser",
        "(Landroid/content/Intent;Ljava/lang/CharSequence;)Landroid/content/Intent;",
        target.m_intent.object<jobject>(), QJniObject::fromString(title).object<jstring>());
    return Intent(std::move(chooser));
}

void Intent::start() const
{
    QJniObject activity(QNativeInterface::QAndroidApplication::context());
    activity.callMethod<void>("startActivity", "(Landroid/content/Intent;)V",
                              m_intent.object<jobject>());
}

QJniObject Intent::jniObject() const { return m_intent; }

}  // namespace android
