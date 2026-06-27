import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Themed.Components

Dialog {
    id: control

    property string dialogTitle: qsTr("Save file")
    property alias nameFilters: fileModel.nameFilters
    property alias fileName: nameField.text

    signal fileAccepted(string path)

    modal: true
    anchors.centerIn: parent
    standardButtons: Dialog.NoButton
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    width: Math.min(parent ? parent.width * 0.9 : 400, 480)
    height: Math.min(parent ? parent.height * 0.85 : 600, 640)
    padding: Theme.padding.large

    onOpened: fileModel.folder = fileModel.homeFolder()

    FileListModel {
        id: fileModel
    }

    background: Rectangle {
        color: Theme.colors.dialogSurface
        radius: Theme.radius.large
        border.color: Theme.colors.border
        border.width: Theme.border.thin
    }

    header: ColumnLayout {
        spacing: Theme.spacing.small

        Text {
            Layout.fillWidth: true
            Layout.topMargin: Theme.padding.large
            Layout.leftMargin: Theme.padding.large
            Layout.rightMargin: Theme.padding.large
            text: control.dialogTitle
            font.pixelSize: Theme.fontSize.large
            font.bold: true
            color: Theme.colors.textPrimary
            wrapMode: Text.WordWrap
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.padding.large
            Layout.rightMargin: Theme.padding.large
            spacing: Theme.spacing.small

            ThemedButton {
                objectName: "fileSaveDialogUpButton"
                text: qsTr("↑ Up")
                buttonStyle: Theme.button.ghost
                buttonSize: Theme.button.small
                enabled: !fileModel.atRoot
                onClicked: fileModel.cdUp()
            }

            Text {
                Layout.fillWidth: true
                text: fileModel.folder
                elide: Text.ElideLeft
                color: Theme.colors.textSecondary
                font.pixelSize: Theme.fontSize.small
            }
        }
    }

    contentItem: ListView {
        id: listView
        objectName: "fileSaveDialogList"
        clip: true
        model: fileModel
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {}

        delegate: ItemDelegate {
            width: ListView.view.width
            objectName: "fileSaveEntry_" + model.name

            contentItem: RowLayout {
                spacing: Theme.spacing.medium

                Text {
                    text: model.isDir ? "📁" : "📄"
                    font.pixelSize: Theme.fontSize.medium
                }

                Text {
                    Layout.fillWidth: true
                    text: model.name
                    elide: Text.ElideRight
                    color: Theme.colors.textPrimary
                    font.pixelSize: Theme.fontSize.medium
                }
            }

            onClicked: {
                if (model.isDir) {
                    fileModel.folder = model.path
                } else {
                    nameField.text = model.name
                }
            }
        }
    }

    footer: Item {
        implicitHeight: footerColumn.implicitHeight + Theme.padding.large * 2

        ColumnLayout {
            id: footerColumn
            anchors.fill: parent
            anchors.margins: Theme.padding.large
            spacing: Theme.spacing.medium

            ThemedInput {
                id: nameField
                objectName: "fileSaveNameField"
                Layout.fillWidth: true
                placeholder: qsTr("File name")
                clearOnSubmit: false
            }

            RowLayout {
                Layout.alignment: Qt.AlignRight
                spacing: Theme.spacing.medium

                ThemedButton {
                    objectName: "fileSaveDialogCancelButton"
                    text: qsTr("Cancel")
                    buttonSize: Theme.button.medium
                    buttonStyle: Theme.button.secondary
                    onClicked: control.close()
                }

                ThemedButton {
                    objectName: "fileSaveDialogSaveButton"
                    text: qsTr("Save")
                    buttonSize: Theme.button.medium
                    enabled: nameField.text.trim() !== ""
                    onClicked: {
                        control.fileAccepted(fileModel.folder + "/" + nameField.text.trim())
                        control.close()
                    }
                }
            }
        }
    }
}
