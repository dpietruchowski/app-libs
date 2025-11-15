import QtQuick
import QtQuick.Controls

import FillIn.Components

Page {
    id: root

    property int contentPadding: Theme.padding.medium
    property bool usePadding: true
    default property alias content: contentArea.data

    background: Rectangle {
        color: Theme.colors.background
    }

    Item {
        id: contentArea
        anchors.fill: parent
        anchors.margins: root.usePadding ? root.contentPadding : 0
    }
}
