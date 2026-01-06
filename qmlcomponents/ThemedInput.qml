import QtQuick
import QtQuick.Controls

import Themed.Components

TextField {
    id: control

    property string placeholder: "Type..."

    signal submitted(string text)

    placeholderText: placeholder
    placeholderTextColor: Theme.colors.textPlaceholder

    color: Theme.colors.textPrimary
    font.pixelSize: Theme.fontSize.medium

    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter

    implicitWidth: Theme.applicationWidth * 0.8
    implicitHeight: 52

    background: Rectangle {
        color: Theme.colors.surface
        radius: Theme.radius.medium
        border.width: control.activeFocus ? Theme.border.medium : Theme.border.thin
        border.color: control.activeFocus ? Theme.colors.primary : Theme.colors.border

        Behavior on border.color { ColorAnimation { duration: 150 } }
        Behavior on border.width { NumberAnimation { duration: 150 } }
    }

    Keys.onReturnPressed: {
        if (text.trim() !== "") {
            submitted(text.trim())
            text = ""
        }
    }

    Keys.onEnterPressed: {
        if (text.trim() !== "") {
            submitted(text.trim())
            text = ""
        }
    }
}
