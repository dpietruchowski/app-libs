import QtQuick
import QtQuick.Controls

Item {
    id: root
    objectName: "sandboxHost"

    property var sandbox: null

    readonly property color chromeBackground: "#141417"
    readonly property color chromeSurface: "#242429"
    readonly property color chromeText: "#e8e8ea"
    readonly property color chromeMuted: "#8a8a93"
    readonly property color chromeAccent: "#5b8cff"
    readonly property color chromeError: "#ff7b72"
    readonly property int chromeHeight: 40

    QtObject {
        id: internal

        property string errorText: ""
        property var instance: null

        function clear() {
            if (instance) {
                instance.destroy()
                instance = null
            }
            errorText = ""
        }

        function rebuild() {
            clear()

            if (!root.sandbox || !root.sandbox.active) {
                errorText = qsTr("Sandbox is not active.")
                return
            }
            if (!root.sandbox.currentFile) {
                errorText = qsTr("No .qml files in %1").arg(root.sandbox.directory)
                return
            }

            var component = Qt.createComponent(root.sandbox.urlFor(root.sandbox.currentFile),
                                               Component.PreferSynchronous)
            if (component.status === Component.Error) {
                errorText = component.errorString()
                component.destroy()
                return
            }

            var object = component.createObject(stage)
            if (!object) {
                errorText = component.errorString()
                component.destroy()
                return
            }

            if (object.anchors)
                object.anchors.fill = stage
            instance = object
            component.destroy()
        }
    }

    Component.onCompleted: internal.rebuild()

    Connections {
        target: root.sandbox
        function onRevisionChanged() { internal.rebuild() }
        function onCurrentFileChanged() { internal.rebuild() }
    }

    Rectangle {
        anchors.fill: parent
        color: root.chromeBackground
    }

    Rectangle {
        id: chrome
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: root.chromeHeight
        color: root.chromeSurface
        z: 1

        ListView {
            id: fileList
            objectName: "sandboxFileList"
            anchors.left: parent.left
            anchors.right: reloadButton.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.leftMargin: 8
            orientation: ListView.Horizontal
            spacing: 4
            clip: true
            model: root.sandbox ? root.sandbox.files : []

            delegate: Rectangle {
                required property int index
                required property string modelData

                readonly property bool current: root.sandbox
                                                && root.sandbox.currentFile === modelData

                objectName: "sandboxFile" + index
                width: label.implicitWidth + 20
                height: root.chromeHeight - 12
                y: (fileList.height - height) / 2
                radius: 4
                color: current ? root.chromeAccent : "transparent"
                border.width: current ? 0 : 1
                border.color: root.chromeMuted

                Text {
                    id: label
                    anchors.centerIn: parent
                    text: parent.modelData.replace(/\.qml$/, "")
                    color: parent.current ? "#ffffff" : root.chromeText
                    font.pixelSize: 12
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: root.sandbox.currentFile = parent.modelData
                }
            }
        }

        Text {
            id: reloadButton
            objectName: "sandboxReloadButton"
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Reload")
            color: root.chromeText
            font.pixelSize: 12

            MouseArea {
                anchors.fill: parent
                anchors.margins: -8
                onClicked: root.sandbox.reload()
            }
        }
    }

    Item {
        id: stage
        objectName: "sandboxStage"
        anchors.top: chrome.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
    }

    Flickable {
        anchors.fill: stage
        contentWidth: width
        contentHeight: errorLabel.implicitHeight + 32
        visible: internal.errorText.length > 0
        clip: true

        Rectangle {
            anchors.fill: parent
            color: root.chromeBackground
        }

        Text {
            id: errorLabel
            objectName: "sandboxError"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 16
            text: internal.errorText
            color: root.chromeError
            font.family: "monospace"
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }
    }
}
