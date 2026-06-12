import QtQuick
import QtQuick.Controls
import Themed.Components

Rectangle {
    id: root
    anchors.fill: parent
    color: Qt.rgba(0, 0, 0, 0.7)
    visible: false
    z: 1000

    property string message: qsTr("You are great")
    property int confettiCount: 50

    signal closed()

    MouseArea {
        anchors.fill: parent
        onClicked: root.close()
    }

    Repeater {
        model: confettiCount

        Item {
            id: confettiItem
            property real startX: Math.random() * root.width
            property real startY: -50
            property real endY: root.height + 50
            property real rotation: Math.random() * 360
            property real duration: 2000 + Math.random() * 2000
            property color confettiColor: {
                var colors = [
                    Theme.colors.primary,
                    Theme.colors.success,
                    Theme.colors.warning,
                    Theme.colors.error,
                    "#FFD700",
                    "#FF69B4",
                    "#00CED1"
                ]
                return colors[Math.floor(Math.random() * colors.length)]
            }
            property real delay: Math.random() * 500
            property real swingAmplitude: 30 + Math.random() * 50
            property real swingSpeed: 1 + Math.random() * 2

            x: startX
            y: startY
            width: 10
            height: 10

            Rectangle {
                anchors.centerIn: parent
                width: parent.width
                height: parent.height
                color: confettiItem.confettiColor
                rotation: confettiItem.rotation
                radius: Math.random() > 0.5 ? 0 : 5
            }

            SequentialAnimation {
                id: confettiAnimation
                running: false

                PauseAnimation {
                    duration: confettiItem.delay
                }

                ParallelAnimation {
                    NumberAnimation {
                        target: confettiItem
                        property: "y"
                        from: confettiItem.startY
                        to: confettiItem.endY
                        duration: confettiItem.duration
                        easing.type: Easing.InQuad
                    }

                    SequentialAnimation {
                        loops: Animation.Infinite
                        NumberAnimation {
                            target: confettiItem
                            property: "x"
                            from: confettiItem.startX
                            to: confettiItem.startX + confettiItem.swingAmplitude
                            duration: 1000 / confettiItem.swingSpeed
                            easing.type: Easing.InOutSine
                        }
                        NumberAnimation {
                            target: confettiItem
                            property: "x"
                            from: confettiItem.startX + confettiItem.swingAmplitude
                            to: confettiItem.startX - confettiItem.swingAmplitude
                            duration: 2000 / confettiItem.swingSpeed
                            easing.type: Easing.InOutSine
                        }
                        NumberAnimation {
                            target: confettiItem
                            property: "x"
                            from: confettiItem.startX - confettiItem.swingAmplitude
                            to: confettiItem.startX
                            duration: 1000 / confettiItem.swingSpeed
                            easing.type: Easing.InOutSine
                        }
                    }

                    NumberAnimation {
                        target: confettiItem
                        property: "rotation"
                        from: 0
                        to: 360 * 3
                        duration: confettiItem.duration
                        easing.type: Easing.Linear
                    }
                }
            }

            Component.onCompleted: {
                if (root.visible) {
                    confettiAnimation.start()
                }
            }

            Connections {
                target: root
                function onVisibleChanged() {
                    if (root.visible) {
                        confettiAnimation.restart()
                    } else {
                        confettiAnimation.stop()
                    }
                }
            }
        }
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.8, 400)
        height: messageText.height + Theme.spacing.xLarge * 2
        color: Theme.colors.background
        radius: Theme.radius.large
        border.color: Theme.colors.primary
        border.width: 3

        Text {
            id: messageText
            anchors.centerIn: parent
            text: root.message
            font.pixelSize: Theme.font.size.xxxLarge
            font.bold: true
            color: Theme.colors.primary
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            width: parent.width - Theme.spacing.large * 2
        }

        SequentialAnimation on scale {
            running: root.visible
            loops: Animation.Infinite
            NumberAnimation {
                from: 1.0
                to: 1.1
                duration: 600
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                from: 1.1
                to: 1.0
                duration: 600
                easing.type: Easing.InOutQuad
            }
        }
    }

    function show() {
        visible = true
    }

    function close() {
        visible = false
        closed()
    }

    opacity: visible ? 1.0 : 0.0
    Behavior on opacity {
        NumberAnimation {
            duration: 300
            easing.type: Easing.InOutQuad
        }
    }
}
