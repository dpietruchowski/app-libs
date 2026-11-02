import QtQuick
import QtQuick.Layouts

import Themed.Components

ColumnLayout {
    id: control

    property string text: ""
    property var bars: []
    property real maximum: 0
    property real valueWidth: 0

    spacing: Theme.chart.barSpacing

    ThemedText {
        Layout.fillWidth: true
        text: control.text
        textStyle: Theme.chart.label
        elide: Text.ElideRight
        maximumLineCount: 1
    }

    Repeater {
        model: control.bars

        delegate: RowLayout {
            required property var modelData
            required property int index

            Layout.fillWidth: true
            spacing: Theme.chart.valueSpacing

            ThemedProgressBar {
                Layout.fillWidth: true
                objectName: control.objectName ? control.objectName + "_bar" + index : ""
                implicitHeight: Theme.chart.groupBarHeight
                from: 0
                to: 1
                value: control.maximum > 0 ? Math.min(1, Math.max(0, modelData.value / control.maximum)) : 0
                progressColor: modelData.color
            }

            ThemedText {
                Layout.preferredWidth: control.valueWidth
                text: modelData.valueText
                textStyle: Theme.chart.value
                horizontalAlignment: Text.AlignRight
            }
        }
    }
}
