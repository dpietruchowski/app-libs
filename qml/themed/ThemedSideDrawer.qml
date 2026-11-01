import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Themed.Components

Drawer {
    id: control

    default property alias drawerContent: contentColumn.data
    property string drawerTitle: ""

    readonly property Item overlayItem: control.Overlay.overlay
    readonly property real safeTopMargin: overlayItem?.SafeArea?.margins.top ?? 0
    readonly property real safeBottomMargin: overlayItem?.SafeArea?.margins.bottom ?? 0
    readonly property real safeLeftMargin: overlayItem?.SafeArea?.margins.left ?? 0

    edge: Qt.LeftEdge
    dragMargin: 0
    parent: Overlay.overlay
    width: (parent ? parent.width : Theme.applicationWidth) * Theme.panel.sideWidthRatio
    height: parent ? parent.height : Theme.applicationHeight
    topPadding: Theme.padding.large + safeTopMargin
    bottomPadding: Theme.padding.large + safeBottomMargin
    leftPadding: Theme.padding.large + safeLeftMargin
    rightPadding: Theme.padding.large
    modal: true
    dim: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Item {
        Rectangle {
            anchors.fill: parent
            anchors.leftMargin: -Theme.radius.xLarge
            color: Theme.colors.dialogSurface
            radius: Theme.radius.xLarge
            border.color: Theme.colors.border
            border.width: Theme.border.thin
        }
    }

    Overlay.modal: Rectangle {
        color: Theme.colors.overlayLight
    }

    contentItem: ColumnLayout {
        id: drawerLayout
        spacing: Theme.spacing.large

        Text {
            Layout.fillWidth: true
            Layout.preferredHeight: contentHeight
            width: drawerLayout.width
            visible: control.drawerTitle !== ""
            text: control.drawerTitle
            color: Theme.colors.textPrimary
            font.pixelSize: Theme.panel.titleSize
            font.bold: true
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            id: contentColumn
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Theme.spacing.large
        }
    }
}
