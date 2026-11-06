import QtQuick

import Themed.Components

ListView {
    id: root

    signal opened(string conversationId)
    signal removeRequested(string conversationId)

    spacing: Theme.spacing.small
    clip: true

    delegate: ThemedListItem {
        id: item

        required property int index
        required property var model

        objectName: "chatConversation" + item.index
        width: root.width
        clickable: true
        removable: true
        title: item.model.title
        subtitle: Qt.formatDateTime(new Date(item.model.updatedAt), Locale.ShortFormat)

        onClicked: root.opened(item.model.conversationId)
        onRemoveClicked: root.removeRequested(item.model.conversationId)
    }
}
