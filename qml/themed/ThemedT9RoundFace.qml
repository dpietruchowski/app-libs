import QtQuick

import Themed.Components

Item {
    id: root

    property Item key
    property string digit: ""

    readonly property real letterRadius: Math.min(width, height) / 3

    Text {
        anchors.centerIn: parent
        text: root.digit
        font.pixelSize: Theme.keyboard.fontSize
        color: root.key.contentColor
    }

    Repeater {
        model: root.key.swipes

        Text {
            required property string modelData
            required property int index

            readonly property real angle: (root.key.swipeArcStart + index * root.key.swipeStep) * Math.PI / 180

            visible: modelData !== root.digit
            x: (root.width - width) / 2 + root.letterRadius * Math.cos(angle)
            y: (root.height - height) / 2 + root.letterRadius * Math.sin(angle)
            text: root.key.displayed(modelData)
            font.pixelSize: Theme.keyboard.hintFontSize
            color: root.key.hintColor
        }
    }
}
