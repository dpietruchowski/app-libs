import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

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

    property real keyboardHeight: 0

    function send(text) {
        if (text.trim() === "")
            return
        root.sent(text)
    }

    Connections {
        target: Qt.inputMethod

        function onKeyboardRectangleChanged() {
            var rawHeight = Qt.inputMethod.keyboardRectangle.height
            root.keyboardHeight = rawHeight > 0 ? rawHeight / Screen.devicePixelRatio : 0
        }
    }

    onKeyboardHeightChanged: chatList.positionViewAtEnd()

    ColumnLayout {
        anchors.fill: parent
        anchors.bottomMargin: root.keyboardHeight
        spacing: Theme.spacing.medium

        Behavior on anchors.bottomMargin {
            NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
        }

        ListView {
            id: chatList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.spacing.small
            model: root.messages

            onCountChanged: positionViewAtEnd()

            delegate: Item {
                width: chatList.width
                height: bubble.height

                readonly property bool isUser: modelData.sender === root.userRole

                Rectangle {
                    id: bubble
                    width: Math.min(chatList.width * 0.82, messageText.implicitWidth + Theme.padding.medium * 2)
                    height: messageText.implicitHeight + Theme.padding.small * 2
                    radius: Theme.radius.large
                    anchors.right: isUser ? parent.right : undefined
                    anchors.left: isUser ? undefined : parent.left
                    color: isUser ? Theme.colors.primary : Theme.colors.surface

                    Text {
                        id: messageText
                        anchors.fill: parent
                        anchors.margins: Theme.padding.small
                        text: modelData.text
                        wrapMode: Text.Wrap
                        color: isUser ? Theme.colors.textInverse : Theme.colors.textPrimary
                        font.pixelSize: Theme.fontSize.medium
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.small
            visible: root.busy

            BusyIndicator {
                running: root.busy
                implicitWidth: 24
                implicitHeight: 24
            }
            Text {
                text: root.typingText
                color: Theme.colors.textSecondary
                font.pixelSize: Theme.fontSize.small
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spacing.small

            ThemedInput {
                id: input
                Layout.fillWidth: true
                placeholder: root.placeholderText
                enabled: !root.busy
                horizontalAlignment: Text.AlignLeft
                onSubmitted: function(text) {
                    root.send(text)
                }
            }

            ThemedButton {
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
