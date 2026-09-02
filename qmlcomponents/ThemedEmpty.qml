import QtQuick
import QtQuick.Layouts

import Themed.Components

Item {
    id: control

    property string text: ""
    property url iconSource: ""

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width, Theme.empty.maxWidth)
        spacing: Theme.empty.spacing

        ThemedIcon {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Theme.empty.iconSize
            Layout.preferredHeight: Theme.empty.iconSize
            visible: control.iconSource.toString() !== ""
            svgSource: control.iconSource
            color: Theme.empty.iconColor
        }

        ThemedText {
            Layout.fillWidth: true
            text: control.text
            horizontalAlignment: Text.AlignHCenter
            textStyle: Theme.text.muted
        }
    }
}
