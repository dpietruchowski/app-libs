import QtQuick

import Themed.Components

Item {
    id: root

    property string text: ""
    property bool fromUser: false
    property string status: "complete"
    property var style: root.fromUser ? Theme.chatBubble.user : Theme.chatBubble.agent
    property real maxWidth: width

    implicitHeight: bubble.height
    opacity: root.status === "complete" || root.status === "running" ? 1 : Theme.opacity.disabled

    Rectangle {
        id: bubble

        objectName: root.objectName !== "" ? root.objectName + "Bubble" : ""
        anchors.left: root.style.alignment === Qt.AlignLeft ? parent.left : undefined
        anchors.right: root.style.alignment === Qt.AlignRight ? parent.right : undefined
        width: label.width + root.style.paddingH * 2
        height: label.height + root.style.paddingV * 2
        radius: root.style.radius
        color: root.style.background
        border.width: root.style.border.a > 0 ? Theme.border.thin : 0
        border.color: root.style.border

        TextMetrics {
            id: metrics

            font: label.font
            text: label.text
        }

        Text {
            id: label

            objectName: root.objectName !== "" ? root.objectName + "Text" : ""
            anchors.centerIn: parent
            width: Math.min(Math.ceil(metrics.advanceWidth) + 1,
                            root.maxWidth - root.style.paddingH * 2)
            text: root.text
            textFormat: root.fromUser ? Text.PlainText : Text.MarkdownText
            wrapMode: Text.Wrap
            lineHeight: 1.25
            color: root.style.text
            font.pixelSize: Theme.chat.fontSize
        }
    }
}
