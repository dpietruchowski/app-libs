import QtQuick
import Themed.Components

Rectangle {
    id: root
    anchors.fill: parent
    color: veilColor
    enabled: false
    z: 1000
    opacity: 0.0
    visible: opacity > 0.0

    property string text: ""
    property int duration: 1200
    property color veilColor: "transparent"
    property color textBackgroundColor: Qt.rgba(Theme.colors.surface.r,
                                                 Theme.colors.surface.g,
                                                 Theme.colors.surface.b, 0.9)

    function show(message) {
        root.text = message
        opacity = 1.0
        hideTimer.restart()
    }

    Timer {
        id: hideTimer
        interval: root.duration
        onTriggered: root.opacity = 0.0
    }

    Behavior on opacity {
        NumberAnimation {
            duration: 300
            easing.type: Easing.InOutQuad
        }
    }

    Rectangle {
        anchors.centerIn: parent
        width: overlayText.width + Theme.spacing.xLarge * 2
        height: overlayText.height + Theme.spacing.large * 2
        radius: Theme.radius.large
        color: root.textBackgroundColor

        Text {
            id: overlayText
            anchors.centerIn: parent
            text: root.text
            font.pixelSize: Theme.fontSize.xxLarge
            font.bold: true
            color: Theme.colors.primary
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            width: Math.min(implicitWidth, root.width - Theme.spacing.xLarge * 4)
        }

        SequentialAnimation on scale {
            running: root.visible
            loops: Animation.Infinite
            NumberAnimation {
                from: 1.0
                to: 1.15
                duration: 400
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                from: 1.15
                to: 1.0
                duration: 400
                easing.type: Easing.InOutQuad
            }
        }
    }
}
