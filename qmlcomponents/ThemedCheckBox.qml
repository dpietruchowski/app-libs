import QtQuick
import QtQuick.Controls

import FillIn.Components

CheckBox {
    id: control

    implicitWidth: contentItem.implicitWidth + indicator.width + Theme.spacing.medium
    implicitHeight: Math.max(contentItem.implicitHeight, indicator.height)

    indicator: Rectangle {
        implicitWidth: 20
        implicitHeight: 20
        x: control.leftPadding
        y: parent.height / 2 - height / 2
        radius: Theme.radius.small
        border.color: control.checked ? Theme.colors.primary : Theme.colors.border
        border.width: Theme.border.medium
        color: control.checked ? Theme.colors.primary : "transparent"

        Behavior on color { ColorAnimation { duration: 120 } }
        Behavior on border.color { ColorAnimation { duration: 120 } }

        ThemedIcon {
            visible: control.checked
            anchors.centerIn: parent
            width: 12
            height: 12
            svgSource: Theme.icons.check
            color: Theme.colors.textInverse
        }
    }

    contentItem: Text {
        text: control.text
        font.pixelSize: Theme.fontSize.medium
        color: Theme.colors.textPrimary
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + Theme.spacing.medium
    }
}
