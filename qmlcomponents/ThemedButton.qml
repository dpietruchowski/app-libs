import QtQuick
import QtQuick.Controls

import Themed.Components

Button {
    id: control

    property var buttonSize: Theme.button.medium
    property var buttonStyle: Theme.button.primary
    property url iconSource: ""
    property bool circular: false

    implicitWidth: buttonSize.width
    implicitHeight: buttonSize.height

    background: Rectangle {
        radius: circular ? Math.min(width, height) / 2 : Theme.radius.medium
        color: !control.enabled ? Theme.colors.surface :
               control.down ? buttonStyle.pressed :
               control.hovered ? buttonStyle.hovered :
               buttonStyle.background
        border.width: Theme.border.thin
        border.color: buttonStyle.border

        Behavior on color { ColorAnimation { duration: 120 } }
    }

    contentItem: Item {
        anchors.fill: parent

        Row {
            spacing: Theme.spacing.small
            anchors.centerIn: parent
            height: parent.height

            ThemedIcon {
                visible: iconSource.toString() !== ""
                svgSource: iconSource
                color: buttonStyle.text
                width: buttonSize.iconSize
                height: buttonSize.iconSize
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                visible: control.text !== ""
                text: control.text
                font.pixelSize: buttonSize.fontSize
                color: buttonStyle.text
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }
}
