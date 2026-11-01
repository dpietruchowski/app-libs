#pragma once

#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUuid>
#include <QVariantMap>
#include <optional>
#include <vector>

#include "chat/domain/chatcontext.h"
#include "chat/domain/conversation.h"
#include "chat/domain/step.h"
#include "chat/ui/chat.h"
#include "chat/ui/chatmessagemodel.h"

namespace chat
{

class ChatSession : public QObject
{
    Q_OBJECT
    Q_PROPERTY(Chat* chat READ chat WRITE setChat NOTIFY chatChanged REQUIRED)
    Q_PROPERTY(QString conversationId READ conversationId WRITE setConversationId NOTIFY
                   conversationIdChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(ChatMessageModel* messages READ messages CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString activity READ activity NOTIFY activityChanged)
    Q_PROPERTY(QVariantMap context READ context WRITE setContext NOTIFY contextChanged)

public:
    explicit ChatSession(QObject* parent = nullptr);

    Chat* chat() const;
    void setChat(Chat* chat);

    QString conversationId() const;
    void setConversationId(const QString& conversationId);

    QString title() const;
    ChatMessageModel* messages();
    bool busy() const;
    QString activity() const;

    QVariantMap context() const;
    void setContext(const QVariantMap& context);

    Q_INVOKABLE void send(const QString& text);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void retry();

signals:
    void chatChanged();
    void conversationIdChanged();
    void titleChanged();
    void busyChanged();
    void activityChanged();
    void contextChanged();

private:
    void attach();
    void detach();
    bool follows(const QUuid& conversationId) const;
    void loadConversation();
    void reloadMessages();
    void adopt(const Conversation& conversation);
    void startTurn(const QUuid& conversationId, const QString& text);
    void clearLiveState();
    void resetState();
    void refreshActivity();
    void setTitle(const QString& title);
    void setBusy(bool busy);
    void setActivity(const QString& activity);

    void onPartialText(const QString& text);
    void onStep(const Step& step);
    void onTurnStarted();
    void onTurnFinished();

    QPointer<Chat> m_chat;
    ChatMessageModel m_messages;
    QList<QMetaObject::Connection> m_connections;
    QUuid m_conversationId;
    QString m_title;
    bool m_busy { false };
    QString m_activity;
    std::optional<ChatContext> m_context;
    std::optional<QString> m_liveText;
    std::vector<Step> m_liveSteps;
};

}
