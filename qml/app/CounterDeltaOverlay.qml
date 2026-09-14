import QtQuick
import Themed.Components

Item {
    id: root
    anchors.fill: parent
    z: 9000

    property string iconSource: ""
    property color accentColor: Theme.colors.accent
    property int stepDuration: 220
    property int holdDuration: 900
    property real topMargin: Theme.scaled(96)

    property bool busy: false
    property int displayedValue: 0
    property int targetValue: 0
    property int delta: 0

    property var queue: []

    function show(previousValue, value) {
        if (previousValue === value)
            return
        queue.push({ from: previousValue, to: value })
        if (!busy)
            startNext()
    }

    function startNext() {
        if (queue.length === 0) {
            pill.visible = false
            busy = false
            return
        }
        busy = true
        const change = queue.shift()
        displayedValue = change.from
        targetValue = change.to
        delta = change.to - change.from
        stepTimer.stepsLeft = Math.abs(delta)
        sequence.restart()
    }

    Rectangle {
        id: pill
        objectName: "counterDeltaPill"
        visible: false
        opacity: 0
        anchors.horizontalCenter: parent.horizontalCenter
        y: root.topMargin
        width: row.implicitWidth + Theme.padding.large * 2
        height: row.implicitHeight + Theme.padding.small * 2
        radius: height / 2
        color: Theme.colors.dialogSurface
        border.width: Theme.border.medium
        border.color: root.accentColor

        Row {
            id: row
            anchors.centerIn: parent
            spacing: Theme.spacing.small

            ThemedIcon {
                id: icon
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.icon.medium
                height: width
                svgSource: root.iconSource
                color: root.accentColor
            }

            Text {
                id: valueText
                objectName: "counterDeltaValue"
                anchors.verticalCenter: parent.verticalCenter
                text: root.displayedValue
                font.pixelSize: Theme.fontSize.xLarge
                font.bold: true
                color: Theme.colors.textPrimary
            }

            Text {
                id: deltaText
                objectName: "counterDeltaText"
                anchors.verticalCenter: parent.verticalCenter
                text: root.delta > 0 ? "+" + root.delta : "−" + Math.abs(root.delta)
                font.pixelSize: Theme.fontSize.large
                font.bold: true
                color: root.delta > 0 ? Theme.colors.success : Theme.colors.error
            }
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
        NumberAnimation { target: valueText; property: "scale"; to: 1.35; duration: root.stepDuration / 2; easing.type: Easing.OutQuad }
        NumberAnimation { target: valueText; property: "scale"; to: 1.0; duration: root.stepDuration / 2; easing.type: Easing.InQuad }
    }

    SequentialAnimation {
        id: sequence
        ScriptAction { script: { pill.visible = true; pill.scale = 0.8 } }
        ParallelAnimation {
            NumberAnimation { target: pill; property: "opacity"; to: 1; duration: 180 }
            NumberAnimation { target: pill; property: "scale"; to: 1; duration: 220; easing.type: Easing.OutBack }
            SequentialAnimation {
                NumberAnimation { target: icon; property: "rotation"; to: -15; duration: 90 }
                NumberAnimation { target: icon; property: "rotation"; to: 15; duration: 120 }
                NumberAnimation { target: icon; property: "rotation"; to: 0; duration: 90 }
            }
        }
        ScriptAction { script: stepTimer.start() }
        PauseAnimation { duration: Math.abs(root.delta) * root.stepDuration + root.holdDuration }
        NumberAnimation { target: pill; property: "opacity"; to: 0; duration: 300 }
        ScriptAction { script: Qt.callLater(root.startNext) }
    }
}
