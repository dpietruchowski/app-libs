import QtQuick
import QtQuick.Controls

import Themed.Components

ProgressBar {
    id: control

    property color progressColor: Theme.colors.primary

    implicitWidth: 200
    implicitHeight: 8

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 8
        color: Theme.colors.divider
        radius: Theme.radius.small
        border.color: Theme.colors.border
        border.width: Theme.border.thin
    }

    contentItem: Item {
        implicitWidth: 200
        implicitHeight: 6

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: Theme.radius.small
            color: progressColor

            Behavior on width {
                NumberAnimation {
                    duration: 250
                    easing.type: Easing.InOutQuad
                }
            }
        }
    }
}
