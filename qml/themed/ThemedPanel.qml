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
    property real heightRatio: 0
    property bool expandable: false
    property bool expanded: false

    readonly property Item overlayItem: control.Overlay.overlay
    readonly property real safeTopMargin: overlayItem?.SafeArea?.margins.top ?? 0
    readonly property real safeBottomMargin: overlayItem?.SafeArea?.margins.bottom ?? 0
    readonly property real safeLeftMargin: overlayItem?.SafeArea?.margins.left ?? 0
    readonly property real safeRightMargin: overlayItem?.SafeArea?.margins.right ?? 0

    property real keyboardInset: typeof KeyboardInset !== "undefined" ? KeyboardInset.bottom : 0

    Behavior on keyboardInset {
        NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
    }

    property real expandProgress: expanded ? 1 : 0

    Behavior on expandProgress {
        NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
    }

    readonly property bool sheetMode: heightRatio > 0
    readonly property real parentHeight: parent ? parent.height : Theme.applicationHeight
    readonly property real maxHeight: Math.min(
        parentHeight * maxHeightRatio + safeBottomMargin + keyboardInset, parentHeight)
    readonly property real sheetHeight: Math.min(
        parentHeight * heightRatio + safeBottomMargin + keyboardInset, parentHeight)

    edge: Qt.BottomEdge
    dragMargin: 0
    parent: Overlay.overlay
    width: parent ? parent.width : Theme.applicationWidth
    height: sheetMode
        ? sheetHeight + (parentHeight - sheetHeight) * expandProgress
        : Math.min(Math.ceil(panelLayout.implicitHeight) + topPadding + bottomPadding, maxHeight)
    topPadding: Theme.padding.small + safeTopMargin * expandProgress
    bottomPadding: Theme.padding.large + safeBottomMargin + keyboardInset
    leftPadding: Theme.padding.large + safeLeftMargin
    rightPadding: Theme.padding.large + safeRightMargin
    modal: true
    dim: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    onClosed: expanded = false

    background: Item {
        Rectangle {
            anchors.fill: parent
            anchors.bottomMargin: -Theme.radius.xLarge
            color: Theme.colors.dialogSurface
            radius: Theme.radius.xLarge * (1 - control.expandProgress)
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

        MouseArea {
            objectName: control.objectName + "Handle"
            Layout.fillWidth: true
            visible: control.handleVisible
            implicitHeight: control.expandable
                ? Math.max(Theme.panel.handleHeight + Theme.spacing.small, Theme.spacing.xLarge)
                : Theme.panel.handleHeight + Theme.spacing.small
            enabled: control.expandable
            onClicked: control.expanded = !control.expanded

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                width: Theme.panel.handleWidth
                height: Theme.panel.handleHeight
                radius: height / 2
                color: Theme.colors.borderStrong
            }
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
            Layout.fillHeight: control.sheetMode
            visible: contentColumn.children.length > 0
            spacing: control.contentSpacing
        }
    }
}
