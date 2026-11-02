import QtQuick

import Themed.Components

Item {
    id: root

    property Item key
    property var flicks: []

    readonly property real inset: Theme.keyboard.keySpacing / 2
    readonly property real sideInset: inset + letterMetrics.descent
    readonly property var places: [Qt.point(0.5, 0), Qt.point(1, 0.5), Qt.point(0.5, 1), Qt.point(0, 0.5)]

    FontMetrics {
        id: letterMetrics
        font.pixelSize: Theme.keyboard.specialFontSize
    }

    Repeater {
        model: root.flicks

        Text {
            required property string modelData
            required property int index

            x: root.sideInset + (root.width - width - 2 * root.sideInset) * root.places[index].x
            y: root.inset + (root.height - height - 2 * root.inset) * root.places[index].y
            text: root.key.displayed(modelData)
            font.pixelSize: Theme.keyboard.specialFontSize
            color: root.key.contentColor
        }
    }
}
