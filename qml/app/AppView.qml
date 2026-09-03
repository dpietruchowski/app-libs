import QtQuick

Item {
    id: root

    default property alias content: contentArea.data

    property color systemBarColor: "transparent"
    property color backgroundColor: "transparent"

    readonly property real safeTopMargin: root.SafeArea?.margins.top ?? 0
    readonly property real safeBottomMargin: root.SafeArea?.margins.bottom ?? 0
    readonly property real safeLeftMargin: root.SafeArea?.margins.left ?? 0
    readonly property real safeRightMargin: root.SafeArea?.margins.right ?? 0

    Rectangle {
        anchors.fill: parent
        color: root.systemBarColor
    }

    Rectangle {
        anchors.fill: contentArea
        color: root.backgroundColor
    }

    Item {
        id: contentArea
        anchors.fill: parent
        anchors.topMargin: root.safeTopMargin
        anchors.bottomMargin: root.safeBottomMargin
        anchors.leftMargin: root.safeLeftMargin
        anchors.rightMargin: root.safeRightMargin
    }
}
