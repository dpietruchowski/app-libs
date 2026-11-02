import QtQuick

import Themed.Components

Item {
    id: root

    property Item key
    property string digit: ""

    readonly property real letterRadius: root.key.circleRadius * 2 / 3
    readonly property real dividerStart: root.key.circleRadius * 0.4
    readonly property var holdOnly: root.key.alternates.filter(character => !root.key.swipes.includes(character))
    readonly property int holdRows: Math.max(1, Math.min(root.holdOnly.length,
                                                         Math.floor(root.key.circleRadius * 1.6 / smallMetrics.height)))

    FontMetrics {
        id: smallMetrics
        font.pixelSize: Theme.fontSize.xSmall
    }

    Repeater {
        model: root.key.ringShown ? root.key.swipes.length + 1 : 0

        Item {
            required property int index

            x: root.width / 2
            y: root.height / 2
            rotation: root.key.swipeArcStart + (index - 0.5) * root.key.swipeStep

            Rectangle {
                x: root.dividerStart
                y: -height / 2
                width: root.key.circleRadius - root.dividerStart
                height: Theme.border.medium
                color: Theme.keyboard.background
            }
        }
    }

    Repeater {
        model: root.key.swipes

        Text {
            required property string modelData
            required property int index

            readonly property bool isDigit: modelData === root.digit
            readonly property real angle: (root.key.swipeArcStart + index * root.key.swipeStep) * Math.PI / 180

            x: (root.width - width) / 2 + root.letterRadius * Math.cos(angle)
            y: (root.height - height) / 2 + root.letterRadius * Math.sin(angle)
            text: root.key.displayed(modelData)
            font.pixelSize: isDigit ? Theme.fontSize.xSmall : Theme.keyboard.specialFontSize
            color: isDigit || root.key.ringShown ? root.key.hintColor : root.key.contentColor
        }
    }

    Grid {
        visible: !root.key.ringShown
        x: (root.width - width) / 2 - root.letterRadius
        y: (root.height - height) / 2
        rows: root.holdRows
        flow: Grid.TopToBottom

        Repeater {
            model: root.holdOnly

            Text {
                required property string modelData

                text: root.key.displayed(modelData)
                font.pixelSize: Theme.fontSize.xSmall
                color: root.key.hintColor
            }
        }
    }
}
