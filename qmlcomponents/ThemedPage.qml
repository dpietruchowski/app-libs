import QtQuick
import QtQuick.Controls

import Themed.Components

Page {
    id: root

    property int contentPadding: Theme.padding.medium
    property bool usePadding: true
    property int maxContentWidth: 400
    default property alias content: contentArea.data

    background: Rectangle {
        color: Theme.colors.background
    }

    Item {
        id: contentArea
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: root.usePadding ? root.contentPadding : 0
        anchors.bottomMargin: root.usePadding ? root.contentPadding : 0
        width: Math.min(parent.width - (root.usePadding ? root.contentPadding * 2 : 0), root.maxContentWidth)
    }
}
