import QtQuick
import QtQuick.Controls
import QtQuick.Window

import Themed.Components

Page {
    id: root

    property int contentPadding: Theme.padding.medium
    property bool usePadding: true
    property int maxContentWidth: 400
    property Component overlayContent: null
    property bool showBackButton: false
    property bool avoidKeyboard: true
    property real keyboardOverlap: 0
    default property alias content: contentArea.data

    signal backClicked()

    function updateKeyboardOverlap() {
        if (!root.avoidKeyboard || !Qt.inputMethod.visible
                || Qt.inputMethod.keyboardRectangle.height <= 0) {
            root.keyboardOverlap = 0
            return
        }
        var keyboardTop = Qt.inputMethod.keyboardRectangle.y / Screen.devicePixelRatio
        var viewBottom = root.mapToItem(null, 0, root.height).y
        root.keyboardOverlap = Math.max(0, viewBottom - keyboardTop)
    }

    onAvoidKeyboardChanged: updateKeyboardOverlap()

    Connections {
        target: Qt.inputMethod
        function onKeyboardRectangleChanged() { root.updateKeyboardOverlap() }
        function onVisibleChanged() { root.updateKeyboardOverlap() }
    }

    background: Rectangle {
        color: Theme.colors.background
    }

    ThemedButton {
        id: backButton
        objectName: "backButton"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: root.contentPadding
        text: qsTr("← Back")
        buttonSize: Theme.button.small
        buttonStyle: Theme.button.secondary
        visible: root.showBackButton
        onClicked: {
            root.backClicked()
            if (typeof stackView !== "undefined" && stackView) stackView.pop()
        }
    }

    Item {
        id: contentArea
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: root.showBackButton ? backButton.bottom : parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: root.usePadding ? root.contentPadding : 0
        anchors.bottomMargin: (root.usePadding ? root.contentPadding : 0) + root.keyboardOverlap
        width: Math.min(parent.width - (root.usePadding ? root.contentPadding * 2 : 0), root.maxContentWidth)

        Behavior on anchors.bottomMargin {
            NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
        }
    }
}
