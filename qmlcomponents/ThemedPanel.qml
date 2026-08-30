import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Themed.Components

Drawer {
    id: control

    default property alias panelContent: contentColumn.data
    property string panelTitle: ""
    property string panelMessage: ""
    property bool handleVisible: true
    property real maxHeightRatio: Theme.panel.maxHeightRatio

    readonly property real maxHeight: (parent ? parent.height : Theme.applicationHeight) * maxHeightRatio

    edge: Qt.BottomEdge
    dragMargin: 0
    parent: Overlay.overlay
    width: parent ? parent.width : Theme.applicationWidth
    height: Math.min(panelLayout.implicitHeight + Theme.padding.small + Theme.padding.large, maxHeight)
    padding: 0
    modal: true
    dim: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Item {
        Rectangle {
            anchors.fill: parent
            anchors.bottomMargin: -Theme.radius.xLarge
            color: Theme.colors.dialogSurface
            radius: Theme.radius.xLarge
            border.color: Theme.colors.border
            border.width: Theme.border.thin
        }
    }

    Overlay.modal: Rectangle {
        color: Theme.colors.overlayLight
    }

    ColumnLayout {
        id: panelLayout
        anchors.fill: parent
        anchors.topMargin: Theme.padding.small
        anchors.bottomMargin: Theme.padding.large
        anchors.leftMargin: Theme.padding.large
        anchors.rightMargin: Theme.padding.large
        spacing: Theme.spacing.large

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: Theme.spacing.small
            visible: control.handleVisible
            implicitWidth: Theme.panel.handleWidth
            implicitHeight: Theme.panel.handleHeight
            radius: height / 2
            color: Theme.colors.borderStrong
        }

        Text {
            Layout.fillWidth: true
            visible: control.panelTitle !== ""
            text: control.panelTitle
            color: Theme.colors.textPrimary
            font.pixelSize: Theme.panel.titleSize
            font.bold: true
            wrapMode: Text.WordWrap
        }

        Text {
            Layout.fillWidth: true
            visible: control.panelMessage !== ""
            text: control.panelMessage
            color: Theme.colors.textSecondary
            font.pixelSize: Theme.fontSize.medium
            wrapMode: Text.WordWrap
        }

        Flickable {
            id: contentFlickable
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: contentColumn.implicitHeight
            Layout.maximumHeight: contentColumn.implicitHeight
            visible: contentColumn.children.length > 0
            contentWidth: width
            contentHeight: contentColumn.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            clip: true

            ColumnLayout {
                id: contentColumn
                width: contentFlickable.width
                spacing: Theme.spacing.large
            }
        }
    }
}
