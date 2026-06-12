import QtQuick
import QtQuick.Controls

import Themed.Components

TextField {
    id: control

    property string placeholder: "Type..."
    property int enterKeyType: Qt.EnterKeyDone
    property bool clearOnSubmit: true

    signal submitted(string text)

    function submit() {
        if (text.trim() !== "") {
            submitted(text.trim())
            if (clearOnSubmit)
                text = ""
        }
    }

    placeholderText: placeholder
    placeholderTextColor: Theme.colors.textPlaceholder

    color: Theme.colors.textPrimary
    font.pixelSize: Theme.fontSize.medium

    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter

    implicitWidth: Theme.applicationWidth * 0.8
    implicitHeight: 52

    inputMethodHints: Qt.ImhNoPredictiveText
    EnterKey.type: control.enterKeyType

    background: Rectangle {
        color: Theme.colors.surface
        radius: Theme.radius.medium
        border.width: control.activeFocus ? Theme.border.medium : Theme.border.thin
        border.color: control.activeFocus ? Theme.colors.primary : Theme.colors.border

        Behavior on border.color { ColorAnimation { duration: 150 } }
        Behavior on border.width { NumberAnimation { duration: 150 } }
    }

    Keys.onReturnPressed: control.submit()
    Keys.onEnterPressed: control.submit()
    onAccepted: control.submit()
}
