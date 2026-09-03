import QtQuick
import QtQuick.Controls

import App.Components
import Themed.Components

Item {
    id: root

    property var pages: []
    property string currentName: ""

    signal pageSelected(string name)

    anchors.fill: parent

    function goBack() {
        if (stackView.depth > 1) {
            stackView.pop()
            return true
        }
        return false
    }

    function push(page) {
        stackView.push(page)
    }

    function selectNext() {
        bottomNav.selectNext()
    }

    function selectPrevious() {
        bottomNav.selectPrevious()
    }

    Loader {
        id: overlayLoader

        anchors.fill: parent
        z: 9999
        sourceComponent: stackView.currentItem?.overlayContent ?? null
    }

    StackView {
        id: stackView

        objectName: "stackView"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: bottomNav.top
        anchors.bottomMargin: (stackView.currentItem?.avoidKeyboard ?? true)
                              ? Math.max(0, KeyboardInset.bottom - bottomNav.height)
                              : 0

        Behavior on anchors.bottomMargin {
            NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
        }

        background: Rectangle {
            color: Theme.colors.background
        }
    }

    ThemedBottomNavigation {
        id: bottomNav

        objectName: "bottomNavigation"
        anchors.bottom: parent.bottom
        width: parent.width
        model: root.pages

        onModelChanged: {
            const index = model.findIndex(item => item.name === root.currentName)
            if (index >= 0)
                currentIndex = index
        }

        onItemSelected: function (index) {
            root.currentName = model[index].name
            root.pageSelected(model[index].name)
        }
    }

    Component.onCompleted: {
        if (root.pages.length === 0)
            return

        stackView.push(root.pages[0].screen)
        bottomNav.currentIndex = 0
        root.currentName = root.pages[0].name
    }
}
