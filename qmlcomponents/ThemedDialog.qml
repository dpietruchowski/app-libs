import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import FillIn.Components

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

    background: Rectangle {
        color: Theme.colors.dialogSurface
        radius: Theme.radius.large
        border.color: Theme.colors.border
        border.width: Theme.border.thin
    }

    header: Rectangle {
        visible: control.title !== ""
        color: "transparent"
        height: visible ? headerText.height + Theme.padding.large * 2 : 0

        Text {
            id: headerText
            anchors.centerIn: parent
            text: control.title
            font.pixelSize: Theme.fontSize.large
            font.bold: true
            color: Theme.colors.textPrimary
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.spacing.large

        Text {
            Layout.fillWidth: true
            text: message
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: Theme.fontSize.medium
            color: Theme.colors.textSecondary
            visible: message !== ""
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: customContent !== null
            children: customContent
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Theme.spacing.medium

            ThemedButton {
                visible: showRejectButton
                text: rejectText
                buttonSize: Theme.button.medium
                buttonStyle: Theme.button.secondary
                onClicked: control.reject()
            }

            ThemedButton {
                text: acceptText
                buttonSize: Theme.button.medium
                buttonStyle: getButtonStyle()
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
