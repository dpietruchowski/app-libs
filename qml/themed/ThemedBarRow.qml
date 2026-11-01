import QtQuick
import QtQuick.Layouts

import Themed.Components

ColumnLayout {
    id: control

    property string text: ""
    property string valueText: ""
    property real value: 0
    property real maximum: 0
    property color barColor: Theme.colors.primary

    readonly property real fraction: maximum > 0 ? Math.min(1, Math.max(0, value / maximum)) : 0

    spacing: Theme.chart.barSpacing

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.chart.valueSpacing

        ThemedText {
            Layout.fillWidth: true
            text: control.text
            textStyle: Theme.chart.label
            elide: Text.ElideRight
            maximumLineCount: 1
        }

        ThemedText {
            text: control.valueText
            textStyle: Theme.chart.value
            visible: control.valueText !== ""
        }
    }

    ThemedProgressBar {
        Layout.fillWidth: true
        implicitHeight: Theme.chart.barHeight
        from: 0
        to: 1
        value: control.fraction
        progressColor: control.barColor
    }
}
