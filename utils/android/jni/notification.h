#pragma once

#include <QJniObject>
#include <QString>

namespace android
{

class Context;

class Notification final
{
public:
    class Builder final
    {
    public:
        explicit Builder(const Context& context, const QString& channelId);

        Builder& setContentTitle(const QString& title);
        Builder& setContentText(const QString& text);
        Builder& setSmallIcon(int resourceId);
        Builder& setAutoCancel(bool autoCancel);

        Notification build() const;

    private:
        QJniObject m_builder;
    };

    QJniObject jniObject() const;

private:
    friend class Builder;
    explicit Notification(QJniObject jni);

    QJniObject m_notification;
};

}  // namespace android
