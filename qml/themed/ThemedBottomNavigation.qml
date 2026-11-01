import QtQuick
import QtQuick.Controls
import Themed.Components

Rectangle {
    id: bottomNav
    height: Theme.navigation.barHeight + bottomInset + handleStripHeight
    color: Theme.colors.surface

    property var model: []
    property int currentIndex: 0
    property int bottomInset: 0
    property bool replacesStack: true
    property bool handleVisible: false
    readonly property int handleStripHeight: handleVisible ? Theme.navigation.handleStripHeight : 0
    signal itemSelected(int index)
    signal handleClicked()

    function select(index) {
        if (index < 0 || index >= model.length)
            return
        if (model[index].action) {
            model[index].action()
            return
        }
        bottomNav.currentIndex = index
        bottomNav.itemSelected(index)
        if (bottomNav.replacesStack && model[index].screen && stackView)
            stackView.replace(null, model[index].screen)
    }

    function selectable(step) {
        let index = bottomNav.currentIndex
        for (let i = 0; i < model.length; ++i) {
            index = (index + step + model.length) % model.length
            if (!model[index].action)
                return index
        }
        return bottomNav.currentIndex
    }

    function selectNext() {
        select(selectable(1))
    }

    function selectPrevious() {
        select(selectable(-1))
    }

    ThemedSeparator {
        width: parent.width
        color: Theme.colors.border
        anchors.top: parent.top
    }

    MouseArea {
        objectName: "navHandle"
        visible: bottomNav.handleVisible
        anchors.top: parent.top
        width: parent.width
        height: bottomNav.handleStripHeight
        cursorShape: Qt.PointingHandCursor
        onClicked: bottomNav.handleClicked()

        Rectangle {
            anchors.centerIn: parent
            width: Theme.panel.handleWidth
            height: Theme.panel.handleHeight
            radius: height / 2
            color: Theme.colors.borderStrong
        }
    }

    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: bottomNav.handleStripHeight + (Theme.navigation.barHeight - height) / 2
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
