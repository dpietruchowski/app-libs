#pragma once

#include <QObject>
#include <QString>

#include "chat/domain/step.h"

namespace chat
{

namespace turn_error
{

    inline const QString kCancelled = QStringLiteral("cancelled");
    inline const QString kUnavailable = QStringLiteral("unavailable");
    inline const QString kTooManyDelegations = QStringLiteral("too_many_delegations");

}

class AgentTurn : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    virtual void cancel() = 0;

signals:
    void sessionReady(const QString& backendSession);
    void partialText(const QString& text);
    void stepStarted(const chat::Step& step);
    void stepFinished(const chat::Step& step);
    void finished(const QString& answer);
    void failed(const QString& errorToken);
};

}
