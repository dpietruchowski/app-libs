#include "chat/ui/errortext.h"

#include <QCoreApplication>

#include "agent/claudecli.h"
#include "chat/application/agentturn.h"

namespace chat
{

QString errorText(MessageStatus status, const QString& errorToken, const QString& loginCommand)
{
    switch (status)
    {
        case MessageStatus::Running:
        case MessageStatus::Complete:
            return QString();
        case MessageStatus::Interrupted:
            return QCoreApplication::translate("ChatErrorText",
                                               "The answer was interrupted when the app closed.");
        case MessageStatus::Failed:
        case MessageStatus::Cancelled:
            break;
    }

    if (errorToken == turn_error::kCancelled)
    {
        return QCoreApplication::translate("ChatErrorText", "Cancelled.");
    }
    if (errorToken == claudecli::kNotLoggedIn)
    {
        return QCoreApplication::translate(
                   "ChatErrorText",
                   "Not logged in to Claude. Run this once in a terminal, then retry:\n%1")
            .arg(loginCommand);
    }
    if (errorToken == claudecli::kNetworkError)
    {
        return QCoreApplication::translate("ChatErrorText", "No connection to the provider.");
    }
    if (errorToken == claudecli::kServerError)
    {
        return QCoreApplication::translate("ChatErrorText", "The provider failed to answer.");
    }
    if (errorToken == claudecli::kTimeout)
    {
        return QCoreApplication::translate("ChatErrorText", "The request timed out.");
    }
    if (errorToken == turn_error::kUnavailable)
    {
        return QCoreApplication::translate("ChatErrorText",
                                           "Claude is not available on this device.");
    }
    return QCoreApplication::translate("ChatErrorText", "The request failed.");
}

}
