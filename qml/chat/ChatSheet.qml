import QtQuick
import QtQuick.Layouts

import Themed.Components

ThemedPanel {
    id: sheet

    required property var session

    readonly property var chat: sheet.session.chat

    function dismiss() {
        if (historyPanel.opened)
            historyPanel.close()
        else
            sheet.close()
    }

    objectName: "chatSheet"
    heightRatio: Theme.panel.sheetHeightRatio
    expandable: true
    leftPadding: sheet.safeLeftMargin
    rightPadding: sheet.safeRightMargin
    closePolicy: Popup.CloseOnPressOutside

    ChatSettingsPanel {
        id: settingsPanel

        model: sheet.chat.model
        reasoningEffort: sheet.chat.reasoningEffort
        onModelChosen: (model) => sheet.chat.model = model
        onReasoningEffortChosen: (effort) => sheet.chat.reasoningEffort = effort
    }

    Item {
        Layout.fillWidth: true
        Layout.fillHeight: true
        focus: true

        Keys.onEscapePressed: sheet.dismiss()
        Keys.onBackPressed: sheet.dismiss()

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.padding.large
            anchors.rightMargin: Theme.padding.large
            spacing: Theme.spacing.medium

            RowLayout {
                objectName: "chatSheetBar"
                Layout.fillWidth: true
                spacing: Theme.spacing.xSmall

                ThemedButton {
                    objectName: "chatHistoryButton"
                    buttonSize: Theme.button.icon
                    buttonStyle: Theme.button.ghost
                    iconSource: Theme.icons.history
                    Accessible.name: qsTr("Conversations")
                    onClicked: historyPanel.open()
                }

                Item {
                    Layout.fillWidth: true
                }

                ThemedButton {
                    objectName: "chatNewButton"
                    buttonSize: Theme.button.small
                    buttonStyle: Theme.button.ghostPrimary
                    pill: true
                    text: qsTr("New")
                    iconSource: Theme.icons.plus
                    iconColor: Theme.colors.primary
                    onClicked: sheet.session.conversationId = ""
                }

                ThemedButton {
                    objectName: "chatSettingsButton"
                    buttonSize: Theme.button.icon
                    buttonStyle: Theme.button.ghost
                    iconSource: Theme.icons.sliders
                    Accessible.name: qsTr("Chat settings")
                    onClicked: settingsPanel.open()
                }
            }

            ChatConversation {
                Layout.fillWidth: true
                Layout.fillHeight: true
                session: sheet.session
            }
        }

        ThemedSidePanel {
            id: historyPanel

            objectName: "chatHistoryPanel"
            anchors.fill: parent
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
                Layout.fillHeight: true
                visible: count > 0
                model: sheet.chat.conversations

                onOpened: (conversationId) => {
                    sheet.session.conversationId = conversationId
                    historyPanel.close()
                }
                onRemoveRequested: (conversationId) => sheet.chat.remove(conversationId)
            }
        }
    }
}
