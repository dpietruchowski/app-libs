import QtQuick
import QtQuick.Layouts

import Themed.Components

ColumnLayout {
    id: control

    property var model: []
    property string textRole: ""
    property string valueRole: ""
    property string colorRole: ""
    property var textFormat: null
    property var valueFormat: null
    property var colorFormat: null

    readonly property int count: repeater.count
    readonly property real maximum: {
        var largest = 0
        for (var i = 0; i < model.length; i++)
            largest = Math.max(largest, control.numberOf(model[i]))
        return largest
    }

    function formatted(hook, role, item) {
        if (hook)
            return hook(item)
        if (role && item[role] !== undefined && item[role] !== null)
            return item[role]
        return ""
    }

    function numberOf(item) {
        return Number(control.formatted(null, control.valueRole, item)) || 0
    }

    spacing: Theme.chart.rowSpacing

    Repeater {
        id: repeater

        model: control.model

        delegate: ThemedBarRow {
            required property var modelData
            required property int index

            Layout.fillWidth: true
            objectName: control.objectName ? control.objectName + "_row" + index : ""
            text: String(control.formatted(control.textFormat, control.textRole, modelData))
            valueText: String(control.formatted(control.valueFormat, control.valueRole, modelData))
            value: control.numberOf(modelData)
            maximum: control.maximum
            barColor: {
                const color = control.formatted(control.colorFormat, control.colorRole, modelData)
                return color === "" ? Theme.colors.primary : color
            }
        }
    }
}
