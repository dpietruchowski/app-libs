import QtQuick
import QtQuick.Layouts

import Themed.Components

ThemedPickerField {
    id: control

    readonly property var parts: control.partsOf(control.value)
    readonly property var hours: Array.from({ length: 24 }, (unused, index) => index)
    readonly property var minutes: Array.from({ length: 12 }, (unused, index) => index * 5)

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
        control.close()
    }

    implicitWidth: Theme.timePicker.fieldWidth
    implicitHeight: Theme.timePicker.fieldHeight
    hasValue: control.parts !== null
    iconSource: Theme.icons.timer
    iconSize: Theme.timePicker.iconSize
    popupContentWidth: Theme.timePicker.columns * Theme.timePicker.cellSize

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
