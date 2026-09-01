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
    property int contentSpacing: Theme.spacing.large
    property real maxHeightRatio: Theme.panel.maxHeightRatio

    readonly property Item overlayItem: control.Overlay.overlay
    readonly property real safeBottomMargin: overlayItem?.SafeArea?.margins.bottom ?? 0
    readonly property real safeLeftMargin: overlayItem?.SafeArea?.margins.left ?? 0
    readonly property real safeRightMargin: overlayItem?.SafeArea?.margins.right ?? 0

    readonly property real maxHeight: (parent ? parent.height : Theme.applicationHeight) * maxHeightRatio + safeBottomMargin

    edge: Qt.BottomEdge
    dragMargin: 0
    parent: Overlay.overlay
    width: parent ? parent.width : Theme.applicationWidth
    height: Math.min(Math.ceil(panelLayout.implicitHeight) + topPadding + bottomPadding, maxHeight)
    topPadding: Theme.padding.small
    bottomPadding: Theme.padding.large + safeBottomMargin
    leftPadding: Theme.padding.large + safeLeftMargin
    rightPadding: Theme.padding.large + safeRightMargin
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

    contentItem: ColumnLayout {
        id: panelLayout
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
            Layout.preferredHeight: contentHeight
            width: panelLayout.width
            visible: control.panelTitle !== ""
            text: control.panelTitle
            color: Theme.colors.textPrimary
            font.pixelSize: Theme.panel.titleSize
            font.bold: true
            wrapMode: Text.WordWrap
        }

        Text {
            Layout.fillWidth: true
            Layout.preferredHeight: contentHeight
            width: panelLayout.width
            visible: control.panelMessage !== ""
            text: control.panelMessage
            color: Theme.colors.textSecondary
            font.pixelSize: Theme.fontSize.medium
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            id: contentColumn
            Layout.fillWidth: true
            visible: contentColumn.children.length > 0
            spacing: control.contentSpacing
        }
    }
}
