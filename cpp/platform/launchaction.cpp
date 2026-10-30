#include "launchaction.h"

#include <QtGlobal>

#include <utility>

#if defined(Q_OS_ANDROID)
#include "android/androidlaunchaction.h"
#else
class LaunchActionListener
{
};
#endif

LaunchAction::LaunchAction(QObject* parent)
    : QObject(parent)
{
#if defined(Q_OS_ANDROID)
    m_listener = std::make_unique<LaunchActionListener>(*this);
#endif
}

LaunchAction::~LaunchAction() = default;

QString LaunchAction::take() { return std::exchange(m_pending, QString()); }

void LaunchAction::deliver(const QString& action)
{
    if (action.isEmpty())
        return;

    m_pending = action;
    emit received(action);
}
