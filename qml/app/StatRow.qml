import QtQuick
import QtQuick.Layouts
import Themed.Components

RowLayout {
    id: root

    property string label: ""
    property string hint: ""
    property string value: ""
    property string text: label + ": " + value
    property color valueColor: Theme.colors.textPrimary
    property int labelFontSize: Theme.fontSize.medium
    property int valueFontSize: Theme.fontSize.medium
    property bool valueBold: true
    property alias valueObjectName: valueText.objectName

    Layout.fillWidth: true
    spacing: Theme.spacing.medium

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 0

        Text {
            text: root.label
            color: Theme.colors.textPrimary
            font.pixelSize: root.labelFontSize
            Layout.fillWidth: true
            elide: Text.ElideRight
        }

        Text {
            text: root.hint
            visible: text !== ""
            color: Theme.colors.textSecondary
            font.pixelSize: Theme.fontSize.small
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
    }

    Text {
        id: valueText
        text: root.value
        color: root.valueColor
        font.pixelSize: root.valueFontSize
        font.bold: root.valueBold
    }
}
