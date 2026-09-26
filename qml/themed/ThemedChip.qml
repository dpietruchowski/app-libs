import QtQuick

import Themed.Components

Rectangle {
    id: control

    property string text: ""
    property bool selected: false
    property color tone: Theme.colors.primary

    signal clicked()

    implicitWidth: label.implicitWidth + Theme.chip.horizontalPadding * 2
    implicitHeight: label.implicitHeight + Theme.chip.verticalPadding * 2
    radius: Theme.chip.radius
    opacity: control.enabled ? 1.0 : Theme.chip.disabledOpacity

    color: {
        if (mouseArea.pressed) {
            return Qt.rgba(control.tone.r, control.tone.g, control.tone.b, Theme.chip.pressedTint)
        }
        if (control.selected) {
            return Qt.rgba(control.tone.r, control.tone.g, control.tone.b, Theme.chip.selectedTint)
        }
        return Theme.colors.cardBackground
    }

    border.width: control.selected ? Theme.chip.selectedBorderWidth : Theme.chip.borderWidth
    border.color: {
        if (control.selected) {
            return control.tone
        }
        return mouseArea.containsMouse ? Theme.colors.cardBorderHover : Theme.colors.cardBorder
    }

    Behavior on color { ColorAnimation { duration: Theme.chip.animationDuration } }
    Behavior on border.color { ColorAnimation { duration: Theme.chip.animationDuration } }
    Behavior on border.width { NumberAnimation { duration: Theme.chip.animationDuration } }
    Behavior on opacity { NumberAnimation { duration: Theme.chip.animationDuration } }

    Text {
        id: label
        anchors.centerIn: parent
        text: control.text
        color: control.selected ? Theme.colors.textPrimary : Theme.colors.textSecondary
        font.pixelSize: Theme.chip.fontSize
        font.bold: control.selected
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: control.clicked()
    }
}
