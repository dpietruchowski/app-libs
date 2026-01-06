import QtQuick
import QtQuick.Controls

import Themed.Components

SpinBox {
    id: control

    font.pixelSize: Theme.fontSize.medium
    editable: true
    implicitHeight: 44

    background: Rectangle {
        color: Theme.colors.surface
        border.color: Theme.colors.cardBorder
        border.width: Theme.border.thin
        radius: Theme.radius.small
    }

    contentItem: TextInput {
        text: control.textFromValue(control.value, control.locale)
        color: Theme.colors.textPrimary
        font: control.font
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        leftPadding: control.mirrored ? control.down.indicator.width  : control.up.indicator.width
        rightPadding: control.mirrored ? control.up.indicator.width  : control.down.indicator.width
        readOnly: true
        validator: control.validator
        inputMethodHints: Qt.ImhFormattedNumbersOnly
    }

    up.indicator: Rectangle {
        x: control.mirrored ? 0 : parent.width - width
        height: parent.height
        implicitWidth: 35
        color: control.up.pressed ? Theme.colors.cardBackground : Theme.colors.surface
        border.color: Theme.colors.cardBorder
        border.width: Theme.border.thin

        Text {
            text: "+"
            font.pixelSize: Theme.fontSize.medium
            color: control.up.pressed ? Theme.colors.primary : Theme.colors.textPrimary
            anchors.centerIn: parent
        }
    }

    down.indicator: Rectangle {
        x: control.mirrored ? parent.width - width : 0
        height: parent.height
        implicitWidth: 35
        color: control.down.pressed ? Theme.colors.cardBackground : Theme.colors.surface
        border.color: Theme.colors.cardBorder
        border.width: Theme.border.thin

        Text {
            text: "-"
            font.pixelSize: Theme.fontSize.medium
            color: control.down.pressed ? Theme.colors.primary : Theme.colors.textPrimary
            anchors.centerIn: parent
        }
    }
}
