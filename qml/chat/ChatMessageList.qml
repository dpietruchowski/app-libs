import QtQuick

import Themed.Components

ListView {
    id: root

    property bool busy: false
    property string activity: ""

    signal retryRequested()

    spacing: Theme.chat.messageSpacing
    clip: true
    onCountChanged: Qt.callLater(positionViewAtEnd)
    onBusyChanged: Qt.callLater(positionViewAtEnd)

    delegate: Column {
        id: entry

        required property int index
        required property string role
        required property string text
        required property string status
        required property string error
        required property var steps

        width: root.width
        spacing: Theme.chat.activitySpacing

        ChatBubble {
            objectName: "chatMessage" + entry.index
            width: parent.width
            visible: entry.text !== ""
            text: entry.text
            fromUser: entry.role === "user"
            status: entry.status
            maxWidth: root.width * Theme.chat.bubbleMaxWidthRatio
        }

        ChatActivity {
            objectName: "chatActivity" + entry.index
            width: parent.width
            steps: entry.steps
        }

        Column {
            objectName: "chatError" + entry.index
            width: parent.width
            visible: entry.error !== ""
            spacing: Theme.chat.errorSpacing

            TextEdit {
                objectName: "chatErrorText" + entry.index
                width: parent.width
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                text: entry.error
                color: Theme.chat.errorText
                font.pixelSize: Theme.chat.errorFontSize
            }

            ThemedButton {
                readonly property bool last: entry.index === root.count - 1

                objectName: last ? "chatRetryButton" : ""
                visible: last && !root.busy
                buttonSize: Theme.button.small
                buttonStyle: Theme.button.ghostPrimary
                pill: true
                text: qsTr("Retry")
                onClicked: root.retryRequested()
            }
        }
    }

    footer: Column {
        width: root.width
        height: visible ? implicitHeight : 0
        topPadding: Theme.chat.messageSpacing
        visible: root.busy

        ChatThinking {
            objectName: "chatThinking"
            width: parent.width
            running: root.busy
            text: root.activity
        }
    }
}
