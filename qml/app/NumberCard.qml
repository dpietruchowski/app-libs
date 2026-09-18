import QtQuick
import QtQuick.Controls
import Themed.Components

ThemedCard {
    id: root

    property string title: "Title"
    property string mainText: ""
    property string mainLabel: ""
    property string subText: ""
    property string subLabel: ""

    clickable: true
    backgroundColor: Theme.colors.surface

    content: Item {
        Text {
            anchors.bottom: mainRow.top
            anchors.bottomMargin: Theme.spacing.small
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.title
            font.pixelSize: Theme.fontSize.xSmall
            color: Theme.colors.textSecondary
        }

        Row {
            id: mainRow
            anchors.centerIn: parent
            spacing: Theme.spacing.small

            Text {
                id: mainValue
                text: root.mainText
                font.pixelSize: Theme.fontSize.xxLarge
                font.bold: true
                color: Theme.colors.textPrimary
            }

            Text {
                visible: root.mainLabel !== ""
                anchors.verticalCenter: mainValue.verticalCenter
                text: root.mainLabel
                font.pixelSize: Theme.fontSize.small
                color: Theme.colors.textSecondary
            }
        }

        Row {
            visible: root.subText !== "" || root.subLabel !== ""
            anchors.top: mainRow.bottom
            anchors.topMargin: 0
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.spacing.xSmall

            Text {
                id: subValue
                text: root.subText
                font.pixelSize: Theme.fontSize.medium
                font.bold: true
                color: Theme.colors.textSecondary
            }

            Text {
                visible: root.subLabel !== ""
                anchors.baseline: subValue.baseline
                text: root.subLabel
                font.pixelSize: Theme.fontSize.small
                color: Theme.colors.textSecondary
            }
        }
    }
}
