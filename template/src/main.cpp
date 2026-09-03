#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "platform/keyboardinsetprovider.h"
#include "platform/systembarstyler.h"
#include "qmlutils/coloredsvgprovider.h"
#include "qmlutils/qmlregistrator.h"

#ifdef LIBS_AUTOMATION
#include "automation/uiautomationserver.h"
#endif

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral(APP_NAME));
    app.setApplicationVersion(QStringLiteral(APP_VERSION));

    QQuickStyle::setStyle("Basic");

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("appVersion", app.applicationVersion());
    engine.rootContext()->setContextProperty("appVersionCode", QStringLiteral(APP_VERSION_CODE));

    KeyboardInsetProvider keyboardInsetProvider;
    SystemBarStyler systemBarStyler;

    QmlRegistrator registrator(engine, "qrc:/" APP_QML_URI "/", APP_QML_URI);
    registrator.registerSingletonInstance("SystemBars", &systemBarStyler);
    registrator.registerSingletonInstance("Themed.Components", "KeyboardInset",
                                          &keyboardInsetProvider);
    registrator.registerSingletonType("Themed.Components", "Theme.qml", "Theme");
    registrator.registerType<ColoredSvgProvider>("Themed.Components", "ColoredSvgProvider");

    engine.load(registrator.getMainQmlUrl());

    if (engine.rootObjects().isEmpty())
        return -1;

#ifdef LIBS_AUTOMATION
    const QByteArray automationPort = qgetenv("APP_AUTOMATION_PORT");
    const quint16 port = automationPort.isEmpty() ? 49200 : automationPort.toUShort();
    new UiAutomationServer(&engine, port, &app);
#endif

    return app.exec();
}
