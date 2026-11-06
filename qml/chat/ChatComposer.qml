import QtQuick
import QtQuick.Controls

import Themed.Components

Item {
    id: root

    property string placeholder: qsTr("Reply")
    property bool busy: false
    readonly property alias text: field.text

    signal sent(string text)
    signal stopRequested()

    function send() {
        if (root.busy || field.text.trim() === "")
            return
        root.sent(field.text.trim())
        field.text = ""
    }

    implicitHeight: Theme.chat.composerHeight

    Rectangle {
        id: pill

        anchors.fill: parent
        radius: Theme.chat.composerRadius
        color: Theme.chat.composerBackground
        border.width: Theme.border.thin
        border.color: field.activeFocus ? Theme.chat.composerBorderFocused
                                        : Theme.chat.composerBorder

        Behavior on border.color { ColorAnimation { duration: 150 } }

        TextField {
            id: field

            objectName: "chatInput"
            anchors.left: parent.left
            anchors.leftMargin: Theme.chat.composerPaddingH
            anchors.right: actionButton.left
            anchors.rightMargin: Theme.spacing.small
            anchors.verticalCenter: parent.verticalCenter
            placeholderText: root.placeholder
            placeholderTextColor: Theme.colors.textPlaceholder
            color: Theme.colors.textPrimary
            font.pixelSize: Theme.chat.composerFontSize
            verticalAlignment: Text.AlignVCenter
            inputMethodHints: Qt.ImhNoPredictiveText
            background: null

            onAccepted: root.send()
        }

        ThemedButton {
            id: actionButton

            objectName: root.busy ? "chatStopButton" : "chatSendButton"
            anchors.right: parent.right
            anchors.rightMargin: (parent.height - height) / 2
            anchors.verticalCenter: parent.verticalCenter
            buttonSize: Theme.button.icon
            buttonStyle: Theme.button.primary
            circular: true
            iconSource: root.busy ? Theme.icons.stop : Theme.icons.send
            Accessible.name: root.busy ? qsTr("Stop") : qsTr("Send")

            onClicked: root.busy ? root.stopRequested() : root.send()
        }
    }
}
