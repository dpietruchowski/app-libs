import QtQuick
import QtQuick.Controls
import Themed.Components

ThemedCard {
    id: root

    property string title: "Title"
    property int number: 10

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
                text: root.number
                font.pixelSize: Theme.fontSize.xxLarge
                font.bold: true
                color: Theme.colors.textPrimary
                horizontalAlignment: Text.AlignHCenter
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }
}
