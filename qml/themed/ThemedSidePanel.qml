import QtQuick
import QtQuick.Layouts

import Themed.Components

Item {
    id: control

    default property alias panelContent: contentColumn.data
    property string panelTitle: ""
    property real widthRatio: Theme.panel.sideWidthRatio
    property real edgeDragWidth: Theme.spacing.large

    readonly property bool opened: motion.targetOpen
    readonly property real position: motion.progress

    function open() {
        motion.settle(true)
    }

    function close() {
        motion.settle(false)
    }

    QtObject {
        id: motion

        readonly property real flingVelocity: 600

        property bool targetOpen: false
        property bool following: false
        property real progress: 0
        property real progressAtGrab: 0
        property real releaseVelocity: 0

        Behavior on progress {
            enabled: !motion.following
            NumberAnimation { duration: 150; easing.type: Easing.OutCubic }
        }

        function settle(open) {
            following = false
            targetOpen = open
            progress = open ? 1 : 0
        }

        function grab() {
            progressAtGrab = progress
            releaseVelocity = 0
            following = true
        }

        function follow(point, travelled) {
            releaseVelocity = point.velocity.x
            progress = Math.max(0, Math.min(1, progressAtGrab + travelled / panel.width))
        }

        function release() {
            const flung = Math.abs(releaseVelocity) > flingVelocity
            settle(flung ? releaseVelocity > 0 : progress > 0.5)
        }
    }

    onVisibleChanged: {
        if (!visible)
            motion.settle(false)
    }

    Rectangle {
        id: scrim
        objectName: control.objectName + "Scrim"
        anchors.fill: parent
        color: Theme.colors.overlayLight
        opacity: control.position
        visible: control.position > 0

        MouseArea {
            anchors.fill: parent
            onClicked: control.close()
        }
    }

    Rectangle {
        id: panel
        objectName: control.objectName + "Panel"
        width: control.width * control.widthRatio
        height: control.height
        x: (control.position - 1) * width
        visible: control.position > 0
        color: Theme.colors.dialogSurface

        MouseArea {
            anchors.fill: parent
        }

        Rectangle {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            width: Theme.border.thin
            color: Theme.colors.border
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.padding.large
            spacing: Theme.spacing.large

            Text {
                Layout.fillWidth: true
                Layout.preferredHeight: contentHeight
                visible: control.panelTitle !== ""
                text: control.panelTitle
                color: Theme.colors.textPrimary
                font.pixelSize: Theme.panel.titleSize
                font.bold: true
                wrapMode: Text.WordWrap
            }

            ColumnLayout {
                id: contentColumn
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.spacing.medium
            }
        }

        DragHandler {
            id: closeDrag
            objectName: control.objectName + "CloseDrag"
            target: null
            enabled: control.opened
            xAxis.enabled: true
            yAxis.enabled: false
            onActiveChanged: active ? motion.grab() : motion.release()
            onCentroidChanged: {
                if (active)
                    motion.follow(centroid, centroid.scenePosition.x - centroid.scenePressPosition.x)
            }
        }
    }

    Item {
        id: edge
        objectName: control.objectName + "Edge"
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: control.edgeDragWidth
        visible: !control.opened

        DragHandler {
            id: openDrag
            objectName: control.objectName + "OpenDrag"
            target: null
            xAxis.enabled: true
            yAxis.enabled: false
            onActiveChanged: active ? motion.grab() : motion.release()
            onCentroidChanged: {
                if (active)
                    motion.follow(centroid, centroid.scenePosition.x - centroid.scenePressPosition.x)
            }
        }
    }
}
