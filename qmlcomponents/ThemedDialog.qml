import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Themed.Components

Dialog {
    id: control

    property string dialogTitle: ""
    property string message: ""
    property string dialogType: "default"
    property string acceptText: qsTr("OK")
    property string rejectText: qsTr("Cancel")
    property bool showRejectButton: false
    property Item customContent: null

    title: dialogTitle
    modal: true
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    closePolicy: Popup.NoAutoClose

    width: Math.min(parent.width * 0.9, 400)
    padding: Theme.padding.large

    background: Rectangle {
        color: Theme.colors.dialogSurface
        radius: Theme.radius.large
        border.color: Theme.colors.border
        border.width: Theme.border.thin
    }

    header: Item {
        visible: control.title !== ""
        implicitHeight: visible ? headerText.implicitHeight + Theme.padding.large * 2 : 0

        Text {
            id: headerText
            anchors.centerIn: parent
            width: parent.width - Theme.padding.large * 2
            text: control.title
            font.pixelSize: Theme.fontSize.large
            font.bold: true
            color: Theme.colors.textPrimary
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.spacing.large

        Text {
            Layout.fillWidth: true
            text: control.message
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: Theme.fontSize.medium
            color: Theme.colors.textSecondary
            visible: control.message !== ""
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: control.customContent ? control.customContent.implicitHeight : 0
            visible: control.customContent !== null
            children: control.customContent ? [control.customContent] : []
        }
    }

    footer: Item {
        implicitHeight: footerRow.implicitHeight + Theme.padding.large * 2

        RowLayout {
            id: footerRow
            anchors.centerIn: parent
            spacing: Theme.spacing.medium

            ThemedButton {
                visible: control.showRejectButton
                text: control.rejectText
                buttonSize: Theme.button.medium
                buttonStyle: Theme.button.secondary
                onClicked: control.reject()
            }

            ThemedButton {
                text: control.acceptText
                buttonSize: Theme.button.medium
                buttonStyle: control.getButtonStyle()
                onClicked: control.accept()
            }
        }
    }

    function getButtonStyle() {
        switch(dialogType) {
            case "success": return Theme.button.success
            case "warning": return Theme.button.secondary
            case "error": return Theme.button.danger
            default: return Theme.button.primary
        }
    }
}
