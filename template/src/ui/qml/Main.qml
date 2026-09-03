import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Themed.Components
import App.Components

ApplicationWindow {
    id: appWindow
    objectName: "appWindow"
    width: 384
    height: 683
    visible: true
    title: qsTr("__APP_NAME__")
    color: Theme.systemBarBackground

    property int clickCount: 0

    Component.onCompleted: {
        Theme.applicationWidth = appWindow.width
        Theme.applicationHeight = appWindow.height
    }

    AppView {
        anchors.fill: parent
        systemBarColor: Theme.systemBarBackground
        backgroundColor: Theme.colors.background

        BackHandler {
            id: backHandler
            objectName: "backHandler"
            anchors.fill: parent
            exitMessage: qsTr("Press back again to exit")

            ThemedPage {
                anchors.fill: parent
                title: qsTr("__APP_NAME__")

                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(parent.width, Theme.contentMaxWidth)
                    spacing: Theme.spacing.large

                    ThemedText {
                        objectName: "greetingText"
                        Layout.alignment: Qt.AlignHCenter
                        textStyle: Theme.text.subtitle
                        text: appWindow.clickCount === 0
                              ? qsTr("__APP_NAME__ is running")
                              : qsTr("Clicked %1 times").arg(appWindow.clickCount)
                    }

                    ThemedButton {
                        objectName: "primaryButton"
                        Layout.alignment: Qt.AlignHCenter
                        text: qsTr("Click me")
                        onClicked: appWindow.clickCount++
                    }

                    ThemedText {
                        objectName: "versionText"
                        Layout.alignment: Qt.AlignHCenter
                        textStyle: Theme.text.caption
                        text: qsTr("version %1 (%2)").arg(appVersion).arg(appVersionCode)
                    }
                }
            }
        }
    }
}
