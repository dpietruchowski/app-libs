import QtQuick
import QtQuick.Layouts

import Themed.Components

ThemedCard {
    id: control

    property string label: ""
    property string value: ""
    property var valueStyle: Theme.stat.value

    margins: Theme.stat.padding
    implicitHeight: Math.max(Theme.stat.minHeight, column.implicitHeight + 2 * Theme.stat.padding)

    content: ColumnLayout {
        id: column

        spacing: Theme.stat.spacing

        ThemedText {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            text: control.value
            textStyle: control.valueStyle
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            maximumLineCount: 1
            wrapMode: Text.NoWrap
        }

        ThemedText {
            Layout.fillWidth: true
            text: control.label
            textStyle: Theme.stat.label
            horizontalAlignment: Text.AlignHCenter
            visible: control.label !== ""
        }
    }
}
