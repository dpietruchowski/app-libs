import QtQuick

FocusScope {
    id: root

    default property alias content: contentArea.data

    property var navigateBack
    property bool quitOnBack: true
    property string exitMessage: ""
    property int exitWindowMs: 2000

    property bool _exitArmed: false

    focus: true

    Keys.onEscapePressed: function(event) {
        event.accepted = true
        if (root.shouldClose())
            Qt.quit()
    }

    function shouldClose() {
        if (root.navigateBack && root.navigateBack())
            return false

        if (!root.quitOnBack)
            return false

        if (Qt.platform.os !== "android")
            return true

        if (root._exitArmed)
            return true

        root._exitArmed = true
        exitTimer.restart()
        return false
    }

    Item {
        id: contentArea
        anchors.fill: parent
    }

    Rectangle {
        id: exitToast
        objectName: "exitToast"
        z: 100000
        visible: opacity > 0
        opacity: root._exitArmed ? 1 : 0
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 40
        width: exitLabel.implicitWidth + 40
        height: exitLabel.implicitHeight + 24
        radius: height / 2
        color: "#DD2A2A2A"

        Text {
            id: exitLabel
            anchors.centerIn: parent
            text: root.exitMessage
            color: "#FFFFFF"
            font.pixelSize: 14
        }

        Behavior on opacity {
            NumberAnimation { duration: 150 }
        }
    }

    Timer {
        id: exitTimer
        interval: root.exitWindowMs
        onTriggered: root._exitArmed = false
    }
}
