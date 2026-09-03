import QtQuick
import QtQuick.Controls
import QtQuick.Window

import Themed.Components

Page {
    id: root

    property int contentPadding: Theme.padding.medium
    property bool usePadding: true
    property int maxContentWidth: Theme.contentMaxWidth
    property Component overlayContent: null
    property bool showBackButton: false
    property bool showTitle: false
    property Component headerAction: null
    property bool avoidKeyboard: true
    default property alias content: contentArea.data

    signal backClicked()

    background: Rectangle {
        color: Theme.colors.background
    }

    ThemedBackButton {
        id: backButton
        anchors.top: parent.top
        anchors.left: contentArea.left
        anchors.topMargin: root.contentPadding
        visible: root.showBackButton
        onClicked: {
            root.backClicked()
            if (typeof stackView !== "undefined" && stackView) stackView.pop()
        }
    }

    Loader {
        id: headerActionLoader
        anchors.top: backButton.top
        anchors.right: contentArea.right
        sourceComponent: root.headerAction
    }

    ThemedText {
        id: titleLabel
        anchors.verticalCenter: backButton.verticalCenter
        anchors.left: root.showBackButton ? backButton.right : contentArea.left
        anchors.leftMargin: root.showBackButton ? Theme.spacing.small : 0
        anchors.right: headerActionLoader.left
        anchors.rightMargin: Theme.spacing.small
        visible: root.showTitle && root.title !== ""
        text: root.title
        textStyle: Theme.page.title
        elide: Text.ElideRight
        maximumLineCount: 1
        verticalAlignment: Text.AlignVCenter
    }

    Item {
        id: contentArea
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: root.showBackButton || root.headerAction || titleLabel.visible
                     ? backButton.bottom : parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: root.usePadding ? root.contentPadding : 0
        anchors.bottomMargin: root.usePadding && !root.footer ? root.contentPadding : 0
        width: Math.min(parent.width - (root.usePadding ? root.contentPadding * 2 : 0), root.maxContentWidth)
    }
}
