import QtQuick
import Themed.Components

Item {
    id: root
    anchors.fill: parent
    z: 9000

    property string iconSource: ""
    property color tint: Theme.colors.accent
    property int iconSize: Theme.scaled(48)
    property int labelFontSize: Theme.fontSize.xMedium
    property int stepDuration: 220
    property int holdDuration: 900
    property Item anchorItem: null
    property bool suppressed: false

    property bool busy: false
    property int displayedValue: 0
    property int delta: 0

    property var queue: []

    function show(previousValue, value) {
        if (previousValue === value)
            return
        queue.push({ from: previousValue, to: value })
        if (!busy)
            startNext()
    }

    function placeMark() {
        if (root.anchorItem) {
            const center = root.anchorItem.mapToItem(root, root.anchorItem.width / 2,
                                                     root.anchorItem.height / 2)
            mark.x = Math.round(center.x - mark.width / 2)
            mark.y = Math.round(center.y - mark.height / 2)
            return
        }
        mark.x = Math.round((root.width - mark.width) / 2)
        mark.y = Math.round(root.height * 2 / 7 - mark.height / 2)
    }

    function startNext() {
        if (queue.length === 0) {
            mark.opacity = 0
            busy = false
            return
        }
        busy = true
        const change = queue.shift()
        displayedValue = change.from
        delta = change.to - change.from
        stepTimer.stepsLeft = Math.abs(delta)
        placeMark()
        sequence.restart()
    }

    Rectangle {
        visible: mark.visible && !root.anchorItem
        opacity: mark.opacity
        scale: mark.scale
        x: mark.x - Theme.padding.large
        y: mark.y - Theme.padding.small
        width: mark.width + Theme.padding.large * 2
        height: mark.height + Theme.padding.small * 2
        radius: height / 2
        color: Theme.colors.dialogSurface
    }

    Row {
        id: mark
        objectName: "counterDeltaPill"
        opacity: 0
        visible: opacity > 0 && !root.suppressed
        spacing: Theme.spacing.small

        onWidthChanged: if (root.busy) root.placeMark()

        ThemedIcon {
            id: icon
            anchors.verticalCenter: parent.verticalCenter
            width: root.iconSize
            height: width
            svgSource: root.iconSource
            color: root.tint
        }

        Text {
            id: valueText
            objectName: "counterDeltaValue"
            anchors.verticalCenter: parent.verticalCenter
            text: root.displayedValue
            color: root.tint
            font.pixelSize: root.labelFontSize
            font.weight: Font.DemiBold
        }
    }

    Timer {
        id: stepTimer
        property int stepsLeft: 0
        interval: root.stepDuration
        repeat: true
        onTriggered: {
            if (stepsLeft <= 0) {
                stop()
                return
            }
            root.displayedValue += root.delta > 0 ? 1 : -1
            stepsLeft -= 1
            bump.restart()
            if (stepsLeft <= 0)
                stop()
        }
    }

    SequentialAnimation {
        id: bump
        NumberAnimation { target: valueText; property: "scale"; to: 1.3; duration: root.stepDuration / 2; easing.type: Easing.OutBack }
        NumberAnimation { target: valueText; property: "scale"; to: 1.0; duration: root.stepDuration / 2; easing.type: Easing.OutCubic }
    }

    SequentialAnimation {
        id: sequence
        ParallelAnimation {
            NumberAnimation { target: mark; property: "opacity"; to: 1; duration: 180; easing.type: Easing.OutCubic }
            NumberAnimation { target: mark; property: "scale"; from: 0.6; to: 1; duration: 260; easing.type: Easing.OutBack }
            SequentialAnimation {
                NumberAnimation { target: icon; property: "rotation"; to: -15; duration: 90 }
                NumberAnimation { target: icon; property: "rotation"; to: 15; duration: 120 }
                NumberAnimation { target: icon; property: "rotation"; to: 0; duration: 90 }
            }
        }
        ScriptAction { script: stepTimer.start() }
        PauseAnimation { duration: Math.abs(root.delta) * root.stepDuration + root.holdDuration }
        NumberAnimation { target: mark; property: "opacity"; to: 0; duration: 300; easing.type: Easing.InQuad }
        ScriptAction { script: Qt.callLater(root.startNext) }
    }
}
