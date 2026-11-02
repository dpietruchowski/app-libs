import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Themed.Components

Control {
    id: control

    property string value: ""
    property string placeholder: ""
    property bool keyNavigable: true

    property int viewYear: 0
    property int viewMonth: 0
    property string today: ""

    readonly property bool hasValue: control.dateOf(control.value) !== null
    readonly property string title: Qt.locale().monthName(control.viewMonth) + " " + control.viewYear
    readonly property var days: control.monthDays(control.viewYear, control.viewMonth)

    signal picked(string value)

    function isoOf(date) {
        return String(date.getFullYear()).padStart(4, "0") + "-"
               + String(date.getMonth() + 1).padStart(2, "0") + "-"
               + String(date.getDate()).padStart(2, "0")
    }

    function dateOf(iso) {
        const match = /^(\d{4})-(\d{2})-(\d{2})$/.exec(iso)
        if (!match)
            return null
        const date = new Date(Number(match[1]), Number(match[2]) - 1, Number(match[3]))
        return control.isoOf(date) === iso ? date : null
    }

    function monthDays(year, month) {
        const lead = (new Date(year, month, 1).getDay() - Qt.locale().firstDayOfWeek + 7) % 7
        const result = []
        for (let i = 0; i < 42; i++) {
            const date = new Date(year, month, 1 - lead + i)
            result.push({ iso: control.isoOf(date), day: date.getDate(), inMonth: date.getMonth() === month })
        }
        return result
    }

    function showMonthOf(iso) {
        const date = control.dateOf(iso) || new Date()
        control.viewYear = date.getFullYear()
        control.viewMonth = date.getMonth()
        control.today = control.isoOf(new Date())
    }

    function step(months) {
        const date = new Date(control.viewYear, control.viewMonth + months, 1)
        control.viewYear = date.getFullYear()
        control.viewMonth = date.getMonth()
    }

    function pick(iso) {
        control.value = iso
        control.picked(iso)
        popup.close()
    }

    function keyActivate() {
        popup.open()
    }

    implicitWidth: Theme.datePicker.fieldWidth
    implicitHeight: Theme.datePicker.fieldHeight
    leftPadding: Theme.padding.medium
    rightPadding: Theme.padding.medium

    Component.onCompleted: control.showMonthOf(control.value)

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
            Layout.preferredWidth: Theme.datePicker.iconSize
            Layout.preferredHeight: Theme.datePicker.iconSize
            svgSource: Theme.icons.calendar
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
        width: 7 * Theme.datePicker.cellSize + leftPadding + rightPadding
        margins: Theme.spacing.medium
        padding: Theme.padding.small
        focus: true

        onAboutToShow: {
            control.showMonthOf(control.value)
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

            RowLayout {
                Layout.fillWidth: true

                ThemedButton {
                    objectName: control.objectName ? control.objectName + "_previous" : ""
                    buttonSize: Theme.button.square
                    buttonStyle: Theme.button.ghost
                    iconSource: Theme.icons.back
                    onClicked: control.step(-1)
                }

                ThemedText {
                    Layout.fillWidth: true
                    text: control.title
                    textStyle: Theme.datePicker.title
                    horizontalAlignment: Text.AlignHCenter
                }

                ThemedButton {
                    objectName: control.objectName ? control.objectName + "_next" : ""
                    buttonSize: Theme.button.square
                    buttonStyle: Theme.button.ghost
                    iconSource: Theme.icons.next
                    onClicked: control.step(1)
                }
            }

            RowLayout {
                spacing: 0

                Repeater {
                    model: 7

                    ThemedText {
                        required property int index

                        Layout.preferredWidth: Theme.datePicker.cellSize
                        text: Qt.locale().dayName((Qt.locale().firstDayOfWeek + index) % 7, Locale.NarrowFormat)
                        textStyle: Theme.datePicker.weekday
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }

            GridLayout {
                columns: 7
                columnSpacing: 0
                rowSpacing: 0

                Repeater {
                    model: control.days

                    Item {
                        id: cell

                        required property var modelData

                        readonly property bool selected: cell.modelData.iso === control.value

                        objectName: control.objectName ? control.objectName + "_day" + cell.modelData.iso : ""
                        Layout.preferredWidth: Theme.datePicker.cellSize
                        Layout.preferredHeight: Theme.datePicker.cellSize

                        Rectangle {
                            anchors.centerIn: parent
                            width: Theme.datePicker.daySize
                            height: Theme.datePicker.daySize
                            radius: Theme.radius.full
                            color: cell.selected ? Theme.datePicker.selectedBackground : "transparent"
                            border.width: cell.modelData.iso === control.today ? Theme.border.thin : 0
                            border.color: Theme.datePicker.todayBorder

                            Text {
                                anchors.centerIn: parent
                                text: cell.modelData.day
                                font.pixelSize: Theme.fontSize.normal
                                color: cell.selected ? Theme.datePicker.selectedText
                                       : cell.modelData.inMonth ? Theme.colors.textPrimary
                                       : Theme.datePicker.outsideText
                            }
                        }

                        TapHandler {
                            onTapped: control.pick(cell.modelData.iso)
                        }
                    }
                }
            }
        }
    }
}
