import QtQuick
import QtQuick.Controls
import Themed.Components

Rectangle {
    id: bottomNav
    height: Theme.navigation.barHeight + bottomInset
    color: Theme.colors.surface

    property var model: []
    property int currentIndex: 0
    property int bottomInset: 0
    signal itemSelected(int index)

    function select(index) {
        if (index < 0 || index >= model.length)
            return
        bottomNav.currentIndex = index
        bottomNav.itemSelected(index)
        if (model[index].screen && stackView)
            stackView.replace(null, model[index].screen)
    }

    function selectNext() {
        select((bottomNav.currentIndex + 1) % model.length)
    }

    function selectPrevious() {
        select((bottomNav.currentIndex + model.length - 1) % model.length)
    }

    Rectangle {
        width: parent.width
        height: Theme.border.thin
        color: Theme.colors.border
        anchors.top: parent.top
    }

    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: (Theme.navigation.barHeight - height) / 2
        spacing: Theme.spacing.medium

        Repeater {
            model: bottomNav.model
            delegate: Rectangle {
                objectName: modelData.name || ""
                width: 55
                height: 50
                color: "transparent"

                property bool active: index === bottomNav.currentIndex
                property color textColor: active ? Theme.colors.primary : Theme.colors.textSecondary
                property bool hasIcon: modelData.icon !== undefined && modelData.icon !== ""

                ThemedIcon {
                    visible: hasIcon
                    svgSource: modelData.icon || ""
                    color: textColor
                    width: Theme.icon.medium
                    height: Theme.icon.medium
                    anchors.centerIn: parent
                }

                Text {
                    visible: !hasIcon && modelData.label !== undefined
                    text: modelData.label || ""
                    color: textColor
                    font.bold: active
                    font.pixelSize: Theme.fontSize.medium
                    anchors.centerIn: parent

                    Behavior on color {
                        ColorAnimation { duration: 150 }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: bottomNav.select(index)
                }
            }
        }
    }
}
