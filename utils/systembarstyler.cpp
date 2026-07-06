#include "systembarstyler.h"

#include <QGuiApplication>

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QStyleHints>
#else
#include <QPalette>
#endif

SystemBarStyler::SystemBarStyler(QObject* parent)
    : QObject(parent)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (qGuiApp)
    {
        connect(qGuiApp->styleHints(), &QStyleHints::colorSchemeChanged, this,
                &SystemBarStyler::systemDarkChanged);
    }
#endif
}

bool SystemBarStyler::systemDark() const
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return qGuiApp && qGuiApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    return qGuiApp && qGuiApp->palette().color(QPalette::Window).lightnessF() < 0.5;
#endif
}
