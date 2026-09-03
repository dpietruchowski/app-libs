import QtQuick
import QtQuick.Controls
import App.Components

ApplicationWindow {
    id: appWindow
    objectName: "appWindow"
    width: 384
    height: 683
    visible: true
    title: qsTr("__APP_NAME__")

    property int clickCount: 0

    AppView {
        anchors.fill: parent
        backgroundColor: "#f5f5f5"

        BackHandler {
            id: backHandler
            objectName: "backHandler"
            anchors.fill: parent

            Column {
                anchors.centerIn: parent
                spacing: 24

                Text {
                    objectName: "greetingText"
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: appWindow.clickCount === 0
                          ? qsTr("__APP_NAME__ is running")
                          : qsTr("Clicked %1 times").arg(appWindow.clickCount)
                    font.pixelSize: 20
                }

                Button {
                    objectName: "primaryButton"
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Click me")
                    onClicked: appWindow.clickCount++
                }

                Text {
                    objectName: "versionText"
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("version %1 (%2)").arg(appVersion).arg(appVersionCode)
                    opacity: 0.6
                }
            }
        }
    }
}
