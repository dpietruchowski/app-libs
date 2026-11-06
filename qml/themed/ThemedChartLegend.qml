import QtQuick
import QtQuick.Layouts

import Themed.Components

RowLayout {
    id: control

    property var entries: []

    spacing: Theme.chart.legendSpacing

    Repeater {
        model: control.entries

        delegate: RowLayout {
            required property var modelData
            required property int index

            objectName: control.objectName ? control.objectName + "_entry" + index : ""
            spacing: Theme.spacing.xSmall

            Rectangle {
                Layout.preferredWidth: Theme.chart.swatchSize
                Layout.preferredHeight: Theme.chart.swatchSize
                radius: Theme.radius.small
                color: modelData.color
            }

            ThemedText {
                text: modelData.name
                textStyle: Theme.chart.legend
            }
        }
    }
}
