import QtQuick
import QtQuick.Layouts

import Themed.Components

ThemedPage {
    id: page

    required property var session

    readonly property var chat: page.session.chat
    readonly property bool available: page.chat.available

    objectName: "pageChat"

    headerAction: Row {
        spacing: Theme.spacing.xSmall

        ThemedButton {
            objectName: "chatNewButton"
            anchors.verticalCenter: parent.verticalCenter
            buttonSize: Theme.button.small
            buttonStyle: Theme.button.ghostPrimary
            pill: true
            text: qsTr("New")
            iconSource: Theme.icons.plus
            iconColor: Theme.colors.primary
            onClicked: page.session.conversationId = ""
        }

        ThemedButton {
            objectName: "chatHistoryButton"
            anchors.verticalCenter: parent.verticalCenter
            buttonSize: Theme.button.icon
            buttonStyle: Theme.button.ghost
            iconSource: Theme.icons.list
            Accessible.name: qsTr("Conversations")
            onClicked: historyPanel.open()
        }

        ThemedButton {
            objectName: "chatSettingsButton"
            anchors.verticalCenter: parent.verticalCenter
            buttonSize: Theme.button.icon
            buttonStyle: Theme.button.ghost
            iconSource: Theme.icons.sliders
            Accessible.name: qsTr("Chat settings")
            onClicked: settingsPanel.open()
        }
    }

    ChatSettingsPanel {
        id: settingsPanel

        model: page.chat.model
        reasoningEffort: page.chat.reasoningEffort
        onModelChosen: (model) => page.chat.model = model
        onReasoningEffortChosen: (effort) => page.chat.reasoningEffort = effort
    }

    ThemedPanel {
        id: historyPanel

        objectName: "chatHistoryPanel"
        panelTitle: qsTr("Conversations")

        ThemedEmpty {
            objectName: "chatHistoryEmpty"
            Layout.fillWidth: true
            visible: historyList.count === 0
            iconSource: Theme.icons.chat
            text: qsTr("No conversations yet")
        }

        ChatConversationList {
            id: historyList

            objectName: "chatConversationList"
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, page.height * Theme.panel.maxHeightRatio)
            visible: count > 0
            model: page.chat.conversations

            onOpened: (conversationId) => {
                page.session.conversationId = conversationId
                historyPanel.close()
            }
            onRemoveRequested: (conversationId) => page.chat.remove(conversationId)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        visible: page.available
        spacing: Theme.spacing.medium

        ChatMessageList {
            objectName: "chatMessageList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: page.session.messages
            busy: page.session.busy
            activity: page.session.activity

            onRetryRequested: page.session.retry()
        }

        ChatComposer {
            objectName: "chatComposer"
            Layout.fillWidth: true
            Layout.bottomMargin: Theme.spacing.small
            busy: page.session.busy

            onSent: (text) => page.session.send(text)
            onStopRequested: page.session.cancel()
        }
    }

    ThemedEmpty {
        objectName: "chatUnavailable"
        anchors.fill: parent
        visible: !page.available
        iconSource: Theme.icons.chat
        text: page.chat.unavailableReason
    }
}
