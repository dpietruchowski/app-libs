import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Themed.Components

Control {
    id: root

    property string title: ""
    property string description: ""
    property alias text: textArea.text
    property bool readOnly: true

    signal copyClicked()
    signal pasteClicked()

    function copyToClipboard() {
        textArea.copyAll()
    }

    function pasteFromClipboard() {
        textArea.pasteFromClipboard()
    }

    Layout.fillWidth: true
    Layout.fillHeight: true

    contentItem: ColumnLayout {
        spacing: Theme.spacing.medium

        RowLayout {
            Layout.fillWidth: true

            Item {
                Layout.fillWidth: true
            }

            ThemedButton {
                text: qsTr("Copy")
                buttonStyle: Theme.button.ghost
                buttonSize: Theme.button.small
                Layout.alignment: Qt.AlignTop
                onClicked: {
                    root.copyToClipboard()
                    root.copyClicked()
                }
            }

            ThemedButton {
                text: qsTr("Paste")
                buttonStyle: Theme.button.ghost
                buttonSize: Theme.button.small
                Layout.alignment: Qt.AlignTop
                visible: !root.readOnly
                onClicked: {
                    root.pasteFromClipboard()
                    root.pasteClicked()
                }
            }
        }

        Label {
            id: titleLabel
            text: root.title
            font.pixelSize: Theme.fontSize.large
            font.bold: true
            color: Theme.colors.textPrimary
        }

        Label {
            id: descriptionLabel
            text: root.description
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            color: Theme.colors.textSecondary
            visible: root.description !== ""
        }

        ThemedTextArea {
            id: textArea
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 200
            readOnly: root.readOnly
            backgroundColor: Theme.colors.cardBackground
        }
    }
}
