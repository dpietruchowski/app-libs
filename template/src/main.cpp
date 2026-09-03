#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

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

    QmlRegistrator registrator(engine, "qrc:/" APP_QML_URI "/", APP_QML_URI);

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
