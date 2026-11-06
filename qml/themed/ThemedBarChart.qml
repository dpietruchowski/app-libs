import QtQuick
import QtQuick.Layouts

import Themed.Components

ColumnLayout {
    id: control

    property var model: []
    property string textRole: ""
    property string valueRole: ""
    property string colorRole: ""
    property var series: []
    property var textFormat: null
    property var valueFormat: null
    property var colorFormat: null
    property var seriesColorFormat: null

    readonly property bool grouped: control.series.length > 0
    readonly property int count: control.grouped ? groups.count : rows.count
    readonly property var roles: control.grouped ? control.series.map(entry => entry.role) : [control.valueRole]
    readonly property real maximum: {
        var largest = 0
        for (var i = 0; i < control.model.length; i++)
            for (var j = 0; j < control.roles.length; j++)
                largest = Math.max(largest, control.numberOf(control.model[i], control.roles[j]))
        return largest
    }
    readonly property real valueWidth: {
        var widest = 0
        for (var i = 0; i < control.model.length; i++)
            for (var j = 0; j < control.roles.length; j++)
                widest = Math.max(widest, metrics.advanceWidth(control.valueTextOf(control.model[i], control.roles[j])))
        return Math.ceil(widest)
    }
    readonly property var legendEntries: control.series.map(entry => ({
        name: entry.name || entry.role,
        color: control.seriesColorOf(entry)
    }))

    function formatted(hook, role, item) {
        if (hook)
            return hook(item, role)
        if (role && item[role] !== undefined && item[role] !== null)
            return item[role]
        return ""
    }

    function numberOf(item, role) {
        return Number(control.formatted(null, role, item)) || 0
    }

    function valueTextOf(item, role) {
        return String(control.formatted(control.valueFormat, role, item))
    }

    function seriesColorOf(entry) {
        if (control.seriesColorFormat)
            return control.seriesColorFormat(entry)
        return entry.color || Theme.colors.primary
    }

    function barsOf(item) {
        return control.series.map(entry => ({
            value: control.numberOf(item, entry.role),
            valueText: control.valueTextOf(item, entry.role),
            color: control.seriesColorOf(entry)
        }))
    }

    spacing: Theme.chart.rowSpacing

    FontMetrics {
        id: metrics

        font.pixelSize: Theme.chart.value.fontSize
        font.bold: Theme.chart.value.bold
    }

    ThemedChartLegend {
        objectName: control.objectName ? control.objectName + "_legend" : ""
        entries: control.legendEntries
        visible: control.grouped
    }

    Repeater {
        id: rows

        model: control.grouped ? [] : control.model

        delegate: ThemedBarRow {
            required property var modelData
            required property int index

            Layout.fillWidth: true
            objectName: control.objectName ? control.objectName + "_row" + index : ""
            text: String(control.formatted(control.textFormat, control.textRole, modelData))
            valueText: control.valueTextOf(modelData, control.valueRole)
            value: control.numberOf(modelData, control.valueRole)
            maximum: control.maximum
            barColor: {
                const color = control.formatted(control.colorFormat, control.colorRole, modelData)
                return color === "" ? Theme.colors.primary : color
            }
        }
    }

    Repeater {
        id: groups

        model: control.grouped ? control.model : []

        delegate: ThemedBarGroup {
            required property var modelData
            required property int index

            Layout.fillWidth: true
            objectName: control.objectName ? control.objectName + "_row" + index : ""
            text: String(control.formatted(control.textFormat, control.textRole, modelData))
            bars: control.barsOf(modelData)
            maximum: control.maximum
            valueWidth: control.valueWidth
        }
    }
}
