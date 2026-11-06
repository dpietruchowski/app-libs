import QtQuick
import QtQuick.Layouts

import Themed.Components

Item {
    id: conversation

    required property var session

    property bool inputEnabled: true

    readonly property var chat: conversation.session.chat
    readonly property bool available: conversation.chat.available

    ColumnLayout {
        anchors.fill: parent
        visible: conversation.available
        spacing: Theme.spacing.medium

        ChatMessageList {
            objectName: "chatMessageList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: conversation.session.messages
            busy: conversation.session.busy
            activity: conversation.session.activity

            onRetryRequested: conversation.session.retry()
        }

        ChatComposer {
            objectName: "chatComposer"
            Layout.fillWidth: true
            Layout.bottomMargin: Theme.spacing.small
            enabled: conversation.inputEnabled
            busy: conversation.session.busy

            onSent: (text) => conversation.session.send(text)
            onStopRequested: conversation.session.cancel()
        }
    }

    ThemedEmpty {
        objectName: "chatUnavailable"
        anchors.fill: parent
        visible: !conversation.available
        iconSource: Theme.icons.chat
        text: conversation.chat.unavailableReason
    }
}
