import QtQuick
import QtQuick.Controls

import Themed.Components

Page {
    id: root

    property int contentPadding: Theme.padding.medium
    property bool usePadding: true
    property int maxContentWidth: 400
    property Component overlayContent: null
    property bool showBackButton: false
    default property alias content: contentArea.data

    signal backClicked()

    background: Rectangle {
        color: Theme.colors.background
    }

    ThemedButton {
        id: backButton
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
        anchors.bottomMargin: root.usePadding ? root.contentPadding : 0
        width: Math.min(parent.width - (root.usePadding ? root.contentPadding * 2 : 0), root.maxContentWidth)
    }
}
