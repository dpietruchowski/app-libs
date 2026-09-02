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
                elide: Text.ElideRight
                maximumLineCount: 1
                textStyle: Theme.text.body
            }

            ThemedText {
                Layout.fillWidth: true
                visible: control.subtitle !== ""
                text: control.subtitle
                elide: Text.ElideRight
                maximumLineCount: 1
                textStyle: Theme.text.caption
            }
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
