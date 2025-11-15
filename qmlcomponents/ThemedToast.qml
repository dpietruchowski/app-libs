import QtQuick
import QtQuick.Controls
import FillIn.Components

Item {
    id: toast

    property string message: ""
    property int duration: 3000
    property string position: "top" // "top" or "bottom"

    anchors.fill: parent
    visible: false
    z: 9999

    Rectangle {
        id: toastBackground
        anchors.horizontalCenter: parent.horizontalCenter
        y: position === "top"
           ? -height
           : parent.height

        width: Math.min(parent.width * 0.9, 400)
        height: toastText.height + Theme.padding.large * 2

        color: Theme.colors.primary
        radius: Theme.radius.medium
        opacity: 0

        Text {
            id: toastText
            anchors.centerIn: parent
            width: parent.width - Theme.padding.large * 2
            text: toast.message
            color: "white"
            font.pixelSize: Theme.fontSize.medium
            font.bold: true
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }

        SequentialAnimation {
            id: showAnimation

            ParallelAnimation {
                NumberAnimation {
                    target: toastBackground
                    property: "y"
                    to: position === "top"
                        ? Theme.padding.large
                        : parent.height - toastBackground.height - Theme.padding.large
                    duration: 300
                    easing.type: Easing.OutCubic
                }
                NumberAnimation {
                    target: toastBackground
                    property: "opacity"
                    to: 1.0
                    duration: 300
                }
            }

            PauseAnimation {
                duration: toast.duration
            }

            ParallelAnimation {
                NumberAnimation {
                    target: toastBackground
                    property: "y"
                    to: position === "top"
                        ? -toastBackground.height
                        : parent.height
                    duration: 300
                    easing.type: Easing.InCubic
                }
                NumberAnimation {
                    target: toastBackground
                    property: "opacity"
                    to: 0
                    duration: 300
                }
            }

            onFinished: {
                toast.visible = false
            }
        }
    }

    function showMessage(msg) {
        if (msg) {
            message = msg
        }
        show()
    }

    function show() {
        if (showAnimation.running) {
            showAnimation.stop()
        }
        visible = true
        showAnimation.start()
    }
}
