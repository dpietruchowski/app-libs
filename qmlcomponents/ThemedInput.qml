import QtQuick
import QtQuick.Controls

import Themed.Components

TextField {
    id: control

    property string placeholder: "Type..."

    signal submitted(string text)

    placeholderText: placeholder

    color: Theme.colors.textPrimary
    font.pixelSize: Theme.fontSize.medium

    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter

    implicitWidth: Theme.applicationWidth * 0.8
    implicitHeight: 52

    PlaceholderText {
        id: placeholderLabel
        text: control.placeholderText
        font: control.font
        color: Theme.colors.textPlaceholder
        verticalAlignment: control.verticalAlignment
        horizontalAlignment: control.horizontalAlignment
        visible: !control.length && !control.activeFocus && (!control.hasOwnProperty("preeditText") || !control.preeditText)
        anchors.fill: parent
        leftPadding: control.leftPadding
        rightPadding: control.rightPadding
        topPadding: control.topPadding
        bottomPadding: control.bottomPadding
    }

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
