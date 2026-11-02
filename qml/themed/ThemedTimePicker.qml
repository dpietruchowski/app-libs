import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Themed.Components

Control {
    id: control

    property string value: ""
    property string placeholder: ""
    property bool keyNavigable: true

    readonly property var parts: control.partsOf(control.value)
    readonly property bool hasValue: control.parts !== null
    readonly property var hours: Array.from({ length: 24 }, (unused, index) => index)
    readonly property var minutes: Array.from({ length: 12 }, (unused, index) => index * 5)

    signal picked(string value)

    function partsOf(text) {
        const match = /^([01]\d|2[0-3]):([0-5]\d)$/.exec(text)
        return match ? { hour: Number(match[1]), minute: Number(match[2]) } : null
    }

    function twoDigits(number) {
        return String(number).padStart(2, "0")
    }

    function commit(hour, minute) {
        control.value = control.twoDigits(hour) + ":" + control.twoDigits(minute)
        control.picked(control.value)
    }

    function pickHour(hour) {
        control.commit(hour, control.hasValue ? control.parts.minute : 0)
    }

    function pickMinute(minute) {
        control.commit(control.hasValue ? control.parts.hour : 0, minute)
        popup.close()
    }

    function keyActivate() {
        popup.open()
    }

    implicitWidth: Theme.timePicker.fieldWidth
    implicitHeight: Theme.timePicker.fieldHeight
    leftPadding: Theme.padding.medium
    rightPadding: Theme.padding.medium

    background: Rectangle {
        color: Theme.colors.surface
        border.color: Theme.colors.cardBorder
        border.width: Theme.border.thin
        radius: Theme.radius.xLarge
    }

    contentItem: RowLayout {
        spacing: Theme.spacing.small

        Text {
            Layout.fillWidth: true
            text: control.hasValue ? control.value : control.placeholder
            color: control.hasValue ? Theme.colors.textPrimary : Theme.colors.textPlaceholder
            font.pixelSize: Theme.fontSize.medium
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        ThemedIcon {
            Layout.preferredWidth: Theme.timePicker.iconSize
            Layout.preferredHeight: Theme.timePicker.iconSize
            svgSource: Theme.icons.timer
            color: Theme.colors.textSecondary
        }
    }

    TapHandler {
        onTapped: control.keyActivate()
    }

    Popup {
        id: popup

        readonly property Item overlayItem: control.Overlay.overlay
        property bool above: false

        objectName: control.objectName ? control.objectName + "_popup" : ""
        y: popup.above ? -popup.implicitHeight - Theme.spacing.xSmall : control.height + Theme.spacing.xSmall
        width: Theme.timePicker.columns * Theme.timePicker.cellSize + leftPadding + rightPadding
        margins: Theme.spacing.medium
        padding: Theme.padding.small
        focus: true

        onAboutToShow: {
            const bottom = control.mapToItem(popup.overlayItem, 0, control.height).y
                           + Theme.spacing.xSmall + popup.implicitHeight + popup.margins
            popup.above = bottom > popup.overlayItem.height
        }

        background: Rectangle {
            color: Theme.colors.surface
            border.color: Theme.colors.cardBorder
            border.width: Theme.border.thin
            radius: Theme.radius.xLarge
        }

        contentItem: ColumnLayout {
            spacing: Theme.spacing.small

            GridLayout {
                columns: Theme.timePicker.columns
                columnSpacing: 0
                rowSpacing: 0

                Repeater {
                    model: control.hours

                    ThemedTimeCell {
                        required property int modelData

                        objectName: control.objectName ? control.objectName + "_hour" + modelData : ""
                        text: control.twoDigits(modelData)
                        selected: control.hasValue && control.parts.hour === modelData
                        onTapped: control.pickHour(modelData)
                    }
                }
            }

            ThemedSeparator {
                Layout.fillWidth: true
            }

            GridLayout {
                columns: Theme.timePicker.columns
                columnSpacing: 0
                rowSpacing: 0

                Repeater {
                    model: control.minutes

                    ThemedTimeCell {
                        required property int modelData

                        objectName: control.objectName ? control.objectName + "_minute" + modelData : ""
                        text: ":" + control.twoDigits(modelData)
                        selected: control.hasValue && control.parts.minute === modelData
                        onTapped: control.pickMinute(modelData)
                    }
                }
            }
        }
    }
}
