import QtQuick
import QtQuick.Layouts

import Themed.Components

RowLayout {
    id: root

    property bool running: false
    property int seconds: 0
    property string text: ""

    spacing: Theme.chat.thinkingSpacing

    onRunningChanged: root.seconds = 0

    Rectangle {
        objectName: root.objectName !== "" ? root.objectName + "Marker" : ""
        Layout.alignment: Qt.AlignVCenter
        implicitWidth: Theme.chat.thinkingMarkerSize
        implicitHeight: Theme.chat.thinkingMarkerSize
        radius: Theme.chat.thinkingMarkerRadius
        color: Theme.chat.thinkingMarker

        SequentialAnimation on opacity {
            running: root.running
            loops: Animation.Infinite
            alwaysRunToEnd: true

            NumberAnimation { to: 0.35; duration: 520; easing.type: Easing.InOutQuad }
            NumberAnimation { to: 1.0; duration: 520; easing.type: Easing.InOutQuad }
        }
    }

    Text {
        objectName: root.objectName !== "" ? root.objectName + "Text" : ""
        Layout.fillWidth: true
        Layout.maximumWidth: implicitWidth
        text: root.text !== "" ? root.text : qsTr("thinking")
        elide: Text.ElideRight
        color: Theme.chat.thinkingText
        font.pixelSize: Theme.chat.thinkingFontSize
        font.family: Theme.chat.thinkingFontFamily
    }

    Text {
        objectName: root.objectName !== "" ? root.objectName + "Detail" : ""
        Layout.minimumWidth: implicitWidth
        visible: root.seconds > 0
        text: root.seconds + "s"
        color: Theme.chat.thinkingDetail
        font.pixelSize: Theme.chat.thinkingFontSize
        font.family: Theme.chat.thinkingFontFamily
    }

    Item {
        Layout.fillWidth: true
    }

    Timer {
        interval: Theme.chat.thinkingTickInterval
        repeat: true
        running: root.running
        onTriggered: root.seconds += 1
    }
}
