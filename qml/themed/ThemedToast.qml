import QtQuick
import QtQuick.Controls
import Themed.Components

Item {
    id: toast

    property string message: ""
    property int duration: 3000
    property string position: "top" // "top" or "bottom"
    property var pendingMessages: []

    anchors.fill: parent
    visible: false
    z: 9999

    Rectangle {
        id: toastBackground
        anchors.horizontalCenter: parent.horizontalCenter
        y: toast.position === "top"
           ? -height
           : toast.height

        width: Math.min(parent.width * 0.9, 400)
        height: toastText.height + Theme.padding.large * 2

        color: Theme.colors.primary
        radius: Theme.radius.medium
        opacity: 0

        MouseArea {
            anchors.fill: parent
            onClicked: toast.dismiss()
        }

        Text {
            id: toastText
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: Theme.padding.large
            anchors.right: closeIcon.left
            anchors.rightMargin: Theme.padding.small
            text: toast.message
            color: "white"
            font.pixelSize: Theme.fontSize.medium
            font.bold: true
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }

        ThemedIcon {
            id: closeIcon
            anchors.right: parent.right
            anchors.rightMargin: Theme.padding.medium
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.icon.small
            height: Theme.icon.small
            svgSource: Theme.icons.close
            color: "white"
        }
    }

    SequentialAnimation {
        id: showAnimation

        ParallelAnimation {
            NumberAnimation {
                target: toastBackground
                property: "y"
                from: toast.position === "top"
                      ? -toastBackground.height
                      : toast.height
                to: toast.position === "top"
                    ? Theme.padding.large
                    : toast.height - toastBackground.height - Theme.padding.large
                duration: 300
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: toastBackground
                property: "opacity"
                from: 0
                to: 1.0
                duration: 300
            }
        }

        PauseAnimation {
            duration: toast.duration
        }

        onFinished: hideAnimation.start()
    }

    ParallelAnimation {
        id: hideAnimation

        NumberAnimation {
            target: toastBackground
            property: "y"
            to: toast.position === "top"
                ? -toastBackground.height
                : toast.height
            duration: 300
            easing.type: Easing.InCubic
        }
        NumberAnimation {
            target: toastBackground
            property: "opacity"
            to: 0
            duration: 300
        }

        onFinished: toast.showNext()
    }

    function showMessage(msg) {
        if (!msg) {
            return
        }
        pendingMessages.push(msg)
        if (!showAnimation.running && !hideAnimation.running) {
            showNext()
        }
    }

    function showNext() {
        if (pendingMessages.length === 0) {
            visible = false
            return
        }
        message = pendingMessages.shift()
        visible = true
        showAnimation.restart()
    }

    function dismiss() {
        if (!showAnimation.running) {
            return
        }
        showAnimation.stop()
        hideAnimation.restart()
    }
}
