import QtQuick
import QtQuick.Layouts

import Themed.Components

ThemedCard {
    id: control

    property string title: ""
    property string subtitle: ""
    property url iconSource: ""
    property bool removable: false
    property bool checkable: false
    property bool checked: false
    property bool done: false
    property string badge: ""
    property var badgeStyle: Theme.badge.neutral

    signal removeClicked()
    signal toggled(bool checked)

    margins: Theme.list.itemPadding
    implicitHeight: Math.max(row.implicitHeight + control.margins * 2, Theme.list.itemHeight)

    RowLayout {
        id: row

        spacing: Theme.spacing.medium

        ThemedCheckBox {
            visible: control.checkable
            checked: control.checked
            onClicked: control.toggled(checked)
        }

        ThemedIcon {
            Layout.preferredWidth: Theme.list.iconSize
            Layout.preferredHeight: Theme.list.iconSize
            visible: control.iconSource.toString() !== ""
            svgSource: control.iconSource
            color: Theme.list.iconColor
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.list.lineSpacing

            ThemedText {
                Layout.fillWidth: true
                text: control.title
                wrapMode: Text.Wrap
                elide: Text.ElideRight
                maximumLineCount: 2
                textStyle: Theme.text.body
                color: control.done ? Theme.list.done.color : textStyle.color
                font.strikeout: control.done && Theme.list.done.strikeout
            }

            ThemedText {
                Layout.fillWidth: true
                visible: control.subtitle !== ""
                text: control.subtitle
                wrapMode: Text.Wrap
                elide: Text.ElideRight
                maximumLineCount: 2
                textStyle: Theme.text.caption
                color: control.done ? Theme.list.done.color : textStyle.color
            }
        }

        ThemedBadge {
            visible: control.badge !== ""
            text: control.badge
            badgeStyle: control.badgeStyle
        }

        ThemedButton {
            visible: control.removable
            iconSource: Theme.icons.remove
            buttonSize: Theme.button.icon
            buttonStyle: Theme.button.ghost
            onClicked: control.removeClicked()
        }
    }
}
