#pragma once

#include <QString>

class AndroidNotification final
{
public:
    enum class Importance
    {
        Low,
        Default,
        High,
        Max,
    };

    static void registerChannel(const QString& channelId, const QString& channelName,
                                Importance importance = Importance::Default);

    explicit AndroidNotification(int id, const QString& channelId);

    AndroidNotification& setTitle(const QString& title);
    AndroidNotification& setText(const QString& text);
    AndroidNotification& setSmallIcon(const QString& drawableName);
    AndroidNotification& setAutoCancel(bool autoCancel);

    void show() const;

private:
    int m_id;
    QString m_channelId;
    QString m_title;
    QString m_text;
    QString m_drawableName { "ic_dialog_info" };
    bool m_autoCancel { true };
};
