import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Themed.Components

Item {
    id: root

    property alias text: textArea.text
    property alias readOnly: textArea.readOnly
    property alias font: textArea.font
    property alias color: textArea.color
    property alias selectionColor: textArea.selectionColor
    property alias selectedTextColor: textArea.selectedTextColor
    property alias wrapMode: textArea.wrapMode

    property color backgroundColor: Theme.colors.surface
    property color textColor: Theme.colors.textPrimary
    property color cursorColor: Theme.colors.primary
    property color scrollBarColor: Theme.colors.border

    implicitWidth: 400
    implicitHeight: flickable.contentHeight

    Rectangle {
        id: bgRect
        anchors.fill: parent
        color: root.backgroundColor
        border.color: textArea.activeFocus ? root.cursorColor : Theme.colors.border
        border.width: textArea.activeFocus ? Theme.border.medium : Theme.border.thin
        radius: Theme.radius.medium

        Behavior on border.color { ColorAnimation { duration: 150 } }
        Behavior on border.width { NumberAnimation { duration: 150 } }
    }

    Flickable {
        id: flickable
        anchors.fill: parent
        anchors.margins: Theme.padding.small
        anchors.rightMargin: vScrollBar.width

        clip: true

        contentHeight: textArea.height
        contentWidth: width

        ScrollBar.vertical: vScrollBar

        TextArea {
            id: textArea
            width: flickable.width

            text: ""
            color: root.textColor
            selectionColor: root.selectionColor
            selectedTextColor: root.selectedTextColor

            selectByMouse: true
            selectByKeyboard: true
            wrapMode: TextEdit.Wrap

            background: null

            topPadding: Theme.padding.small
            leftPadding: Theme.padding.small
            rightPadding: Theme.padding.small
            bottomPadding: Theme.padding.small

            font.pixelSize: Theme.fontSize.medium

            cursorDelegate: Rectangle {
                visible: textArea.activeFocus && !textArea.readOnly
                width: Theme.border.medium
                color: root.cursorColor
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.IBeamCursor
                acceptedButtons: Qt.RightButton
                onClicked: (mouse) => {
                    textArea.forceActiveFocus()
                    if (mouse.button === Qt.RightButton) contextMenu.popup()
                }
            }

            Menu {
                id: contextMenu
                MenuItem { text: "Cut"; enabled: !textArea.readOnly; onTriggered: textArea.cut() }
                MenuItem { text: "Copy"; onTriggered: textArea.copy() }
                MenuItem { text: "Paste"; enabled: !textArea.readOnly; onTriggered: textArea.paste() }
                MenuSeparator {}
                MenuItem { text: "Select All"; onTriggered: textArea.selectAll() }
            }
        }
    }

    ScrollBar {
        id: vScrollBar

        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.rightMargin: Theme.padding.small / 2
        anchors.topMargin: Theme.padding.small
        anchors.bottomMargin: Theme.padding.small

        policy: ScrollBar.AsNeeded
        visible: flickable.contentHeight > flickable.height

        background: Rectangle {
            color: "transparent"
            width: 8
        }

        contentItem: Rectangle {
            implicitWidth: 8
            implicitHeight: 100
            radius: Theme.radius.small
            color: vScrollBar.pressed ? Qt.darker(root.scrollBarColor, 1.2) : root.scrollBarColor
            opacity: vScrollBar.active || vScrollBar.pressed ? 1.0 : 0.5

            Behavior on opacity { NumberAnimation { duration: 200 } }
        }
    }
}
