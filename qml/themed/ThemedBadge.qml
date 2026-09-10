import QtQuick

import Themed.Components

Rectangle {
    id: control

    property string text: ""
    property var badgeStyle: Theme.badge.neutral

    implicitWidth: label.implicitWidth + 2 * Theme.badge.paddingH
    implicitHeight: Theme.badge.height
    radius: Theme.badge.radius
    color: control.badgeStyle.background

    Text {
        id: label

        anchors.centerIn: parent
        text: control.text
        color: control.badgeStyle.text
        font.pixelSize: Theme.badge.fontSize
        font.bold: true
    }
}
