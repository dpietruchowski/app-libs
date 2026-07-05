#include "systembarstyler.h"

#include <QEvent>
#include <QGuiApplication>
#include <QPalette>

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
