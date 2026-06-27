import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Themed.Components

Item {
    id: root

    // Model of chat bubbles. Each item is expected to expose `sender`
    // (the bubble is right-aligned when it equals `userRole`) and `text`.
    property var messages: []
    property bool busy: false

    property string userRole: "user"
    property string placeholderText: qsTr("Write a message…")
    property string typingText: qsTr("Typing…")
    property url sendIcon: Theme.icons.send

    // Emitted when the user submits a non-empty message.
    signal sent(string text)

    function send(text) {
        if (text.trim() === "")
            return
        root.sent(text)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.spacing.medium

        ListView {
            id: chatList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.spacing.small
            model: root.messages

            onCountChanged: positionViewAtEnd()

            delegate: Item {
                objectName: "chatMessage" + index
                width: chatList.width
                height: bubble.height

                readonly property bool isUser: modelData.sender === root.userRole
                property alias text: messageText.text

                Rectangle {
                    id: bubble
                    readonly property real maxTextWidth: chatList.width * 0.82 - Theme.padding.small * 2
                    width: messageText.width + Theme.padding.small * 2
                    height: messageText.height + Theme.padding.small * 2
                    radius: Theme.radius.large
                    anchors.right: isUser ? parent.right : undefined
                    anchors.left: isUser ? undefined : parent.left
                    color: isUser ? Theme.colors.primary : Theme.colors.surface

                    TextMetrics {
                        id: messageMetrics
                        font: messageText.font
                        text: messageText.text
                    }

                    Text {
                        id: messageText
                        anchors.centerIn: parent
                        width: Math.min(Math.ceil(messageMetrics.advanceWidth) + 1, bubble.maxTextWidth)
                        text: modelData.text
                        wrapMode: Text.Wrap
                        color: isUser ? Theme.colors.textInverse : Theme.colors.textPrimary
                        font.pixelSize: Theme.fontSize.medium
                    }
                }
            }
        }

        Rectangle {
            id: typingBubble
            Layout.alignment: Qt.AlignLeft
            visible: root.busy
            implicitWidth: typingRow.width + Theme.padding.small * 2
            implicitHeight: typingRow.height + Theme.padding.small * 2
            radius: Theme.radius.large
            color: Theme.colors.surface

            RowLayout {
                id: typingRow
                anchors.centerIn: parent
                spacing: Theme.spacing.small

                Row {
                    spacing: Theme.spacing.xSmall
                    Layout.alignment: Qt.AlignVCenter

                    Repeater {
                        model: 3

                        Rectangle {
                            width: 6
                            height: 6
                            radius: 3
                            color: Theme.colors.textSecondary

                            SequentialAnimation on opacity {
                                running: typingBubble.visible
                                loops: Animation.Infinite
                                PauseAnimation { duration: index * 160 }
                                NumberAnimation { to: 1.0; duration: 300; easing.type: Easing.InOutQuad }
                                NumberAnimation { to: 0.3; duration: 300; easing.type: Easing.InOutQuad }
                                PauseAnimation { duration: (2 - index) * 160 }
                            }

                            SequentialAnimation on y {
                                running: typingBubble.visible
                                loops: Animation.Infinite
                                PauseAnimation { duration: index * 160 }
                                NumberAnimation { to: -3; duration: 300; easing.type: Easing.OutQuad }
                                NumberAnimation { to: 0; duration: 300; easing.type: Easing.InQuad }
                                PauseAnimation { duration: (2 - index) * 160 }
                            }
                        }
                    }
                }

                Text {
                    text: root.typingText
                    color: Theme.colors.textSecondary
                    font.pixelSize: Theme.fontSize.small
                    Layout.alignment: Qt.AlignVCenter
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.small

            ThemedInput {
                id: input
                objectName: "chatInput"
                Layout.fillWidth: true
                placeholder: root.placeholderText
                enabled: !root.busy
                clearOnSubmit: true
                horizontalAlignment: Text.AlignLeft
                onSubmitted: function(text) {
                    root.send(text)
                }
            }

            ThemedButton {
                objectName: "chatSendButton"
                Layout.alignment: Qt.AlignVCenter
                buttonSize: Theme.button.square
                iconSource: root.sendIcon
                enabled: !root.busy && input.text.trim() !== ""
                onClicked: {
                    root.send(input.text)
                    input.text = ""
                }
            }
        }
    }
}
