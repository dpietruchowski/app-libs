import QtQuick
import QtQuick.Controls
import Themed.Components

Rectangle {
    id: overlay
    anchors.fill: parent
    color: Qt.rgba(0, 0, 0, 0.7)
    z: 1000

    property string message: ""
    property int progress: 0
    property int total: 0

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onClicked: {}
        onPressed: {}
        onReleased: {}
        onPositionChanged: {}
    }

    Column {
        anchors.centerIn: parent
        spacing: Theme.spacing.large

        BusyIndicator {
            anchors.horizontalCenter: parent.horizontalCenter
            running: overlay.visible
            width: 64
            height: 64
        }

        Text {
            text: overlay.message
            color: Theme.colors.textPrimary
            font.pixelSize: Theme.fontSize.large
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            text: overlay.total > 0 ? overlay.progress + " / " + overlay.total : ""
            color: Theme.colors.textSecondary
            font.pixelSize: Theme.fontSize.medium
            horizontalAlignment: Text.AlignHCenter
            visible: overlay.total > 0
        }

        ThemedProgressBar {
            width: Theme.applicationWidth * 0.6
            progressColor: Theme.colors.primary
            from: 0
            to: overlay.total
            value: overlay.progress
            visible: overlay.total > 0
        }
    }
}
