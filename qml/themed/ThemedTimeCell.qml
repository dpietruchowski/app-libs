import QtQuick
import QtQuick.Layouts

import Themed.Components

Item {
    id: control

    property string text: ""
    property bool selected: false

    signal tapped

    Layout.preferredWidth: Theme.timePicker.cellSize
    Layout.preferredHeight: Theme.timePicker.cellSize

    Rectangle {
        anchors.centerIn: parent
        width: Theme.timePicker.valueSize
        height: Theme.timePicker.valueSize
        radius: Theme.radius.full
        color: control.selected ? Theme.timePicker.selectedBackground : "transparent"

        Text {
            anchors.centerIn: parent
            text: control.text
            font.pixelSize: Theme.fontSize.normal
            color: control.selected ? Theme.timePicker.selectedText : Theme.colors.textPrimary
        }
    }

    TapHandler {
        onTapped: control.tapped()
    }
}
