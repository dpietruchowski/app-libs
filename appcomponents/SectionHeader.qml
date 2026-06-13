import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Controls.impl
import Themed.Components

Item {
    id: root
    implicitHeight: Math.max(row.implicitHeight, Theme.button.square.size)
    implicitWidth: row.implicitWidth

    property string title
    property alias titleFont: titleText.font
    property bool expandable: false
    property bool expanded: true
    default property alias actions: actionsRow.data

    MouseArea {
        anchors.fill: parent
        enabled: root.expandable
        cursorShape: root.expandable ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: root.expanded = !root.expanded
    }

    RowLayout {
        id: row
        anchors.fill: parent
        spacing: Theme.spacing.medium

        Item {
            visible: root.expandable
            Layout.preferredWidth: Theme.icon.small
            Layout.preferredHeight: Theme.icon.small

            ThemedIcon {
                anchors.fill: parent
                svgSource: root.expanded ? Theme.icons.collapse : Theme.icons.expand
                color: Theme.colors.textSecondary
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                hoverEnabled: true
                onClicked: root.expanded = !root.expanded
                ToolTip.visible: containsMouse
                ToolTip.text: root.expanded ? "Collapse" : "Expand"
                ToolTip.delay: 500
            }
        }

        Text {
            id: titleText
            text: root.title
            font.pixelSize: Theme.fontSize.small
            font.bold: true
            font.letterSpacing: 1.5
            color: Theme.colors.textSecondary
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.border.thin
            color: Theme.colors.border
        }

        Row {
            id: actionsRow
            spacing: Theme.spacing.small
        }
    }
}
