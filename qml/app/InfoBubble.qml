import QtQuick
import Themed.Components

Item {
    id: root

    property Item target: null
    property string title: ""
    property string text: ""
    property string buttonText: qsTr("Got it")
    property real gap: Theme.spacing.small
    property real borderWidth: Theme.border.thin
    property color borderColor: Theme.button.ghost.border

    property rect anchorRect: Qt.rect(0, 0, 0, 0)

    signal dismissed()

    anchors.fill: parent
    z: 1000
    opacity: 0.0
    visible: opacity > 0.0

    function show(targetItem) {
        if (targetItem)
            root.target = targetItem
        root.updateAnchor()
        root.opacity = 1.0
    }

    function hide() {
        if (root.opacity === 0.0)
            return
        root.opacity = 0.0
        root.dismissed()
    }

    function updateAnchor() {
        if (!root.target) {
            root.anchorRect = Qt.rect(root.width / 2, root.height / 2, 0, 0)
            return
        }
        root.anchorRect = root.target.mapToItem(root, 0, 0, root.target.width, root.target.height)
    }

    onWidthChanged: if (visible) updateAnchor()
    onHeightChanged: if (visible) updateAnchor()

    Behavior on opacity {
        NumberAnimation {
            duration: 200
            easing.type: Easing.InOutQuad
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.hide()
    }

    Item {
        id: bubble

        readonly property bool below: root.anchorRect.y + root.anchorRect.height + root.gap + arrow.height + height
                                      < root.height - Theme.spacing.large
        readonly property real margin: Theme.spacing.medium
        readonly property real anchorCenterX: root.anchorRect.x + root.anchorRect.width / 2

        width: Math.min(root.width - margin * 2, Theme.contentMaxWidth)
        height: bubbleBody.height
        x: Math.max(margin, Math.min(anchorCenterX - width / 2, root.width - width - margin))
        y: below ? root.anchorRect.y + root.anchorRect.height + root.gap + arrow.height / 2
                 : root.anchorRect.y - root.gap - arrow.height / 2 - height

        MouseArea {
            z: -2
            anchors.fill: parent
        }

        Rectangle {
            id: arrow
            readonly property real diagonal: width * Math.SQRT2
            width: Theme.spacing.medium * 1.4
            height: width
            rotation: 45
            color: bubbleBody.color
            border.width: root.borderWidth
            border.color: root.borderColor
            x: Math.max(Theme.radius.large,
                        Math.min(bubble.anchorCenterX - bubble.x - width / 2,
                                 bubble.width - width - Theme.radius.large))
            y: bubble.below ? -height / 2 : bubbleBody.height - height / 2
        }

        Rectangle {
            id: bubbleBody
            z: -1
            width: parent.width
            height: bubbleColumn.height + Theme.spacing.large * 2
            radius: Theme.radius.large
            color: Theme.colors.surface
            border.width: root.borderWidth
            border.color: root.borderColor

            Column {
                id: bubbleColumn
                x: Theme.spacing.large
                y: Theme.spacing.large
                width: parent.width - Theme.spacing.large * 2
                spacing: Theme.spacing.small

                Text {
                    width: parent.width
                    visible: root.title !== ""
                    text: root.title
                    color: Theme.colors.textPrimary
                    font.pixelSize: Theme.fontSize.medium
                    font.bold: true
                    wrapMode: Text.WordWrap
                }

                Text {
                    width: parent.width
                    text: root.text
                    textFormat: Text.StyledText
                    color: Theme.colors.textSecondary
                    font.pixelSize: Theme.fontSize.small
                    wrapMode: Text.WordWrap
                    lineHeight: 1.2
                }

                Item {
                    width: parent.width
                    height: gotItButton.height

                    ThemedButton {
                        id: gotItButton
                        objectName: root.objectName === "" ? "" : root.objectName + "Button"
                        anchors.right: parent.right
                        text: root.buttonText
                        buttonStyle: Theme.button.primary
                        onClicked: root.hide()
                    }
                }
            }
        }

        Rectangle {
            x: arrow.x + arrow.width / 2 - width / 2
            y: bubble.below ? root.borderWidth : bubbleBody.height - root.borderWidth - height
            width: arrow.diagonal - root.borderWidth * 3
            height: arrow.diagonal / 2
            color: bubbleBody.color
        }
    }
}
