import QtQuick
import QtQuick.Controls
import Themed.Components

ThemedCard {
    id: root

    property string title: "Title"
    property int number: 10
    property bool done: false
    property string text: title + ": " + number + (done ? " ✓" : "")

    property real displayNumber: number
    Behavior on displayNumber {
        NumberAnimation {
            duration: 600
            easing.type: Easing.OutCubic
        }
    }

    clickable: true
    backgroundColor: Theme.colors.surface

    content: Item {
        Column {
            anchors.centerIn: parent
            spacing: Theme.spacing.small
            Text {
                text: root.title
                font.pixelSize: Theme.fontSize.xSmall
                color: Theme.colors.textSecondary
                horizontalAlignment: Text.AlignHCenter
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                text: Math.round(root.displayNumber)
                font.pixelSize: Theme.fontSize.xxLarge
                font.bold: true
                color: Theme.colors.textPrimary
                horizontalAlignment: Text.AlignHCenter
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }
}
