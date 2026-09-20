import QtQuick
import Themed.Components

Row {
    id: root

    property var dailyStages: []
    property var dueCounts: []
    property int todayIndex: -1
    property real cellSize: Theme.activity.weekCellSize

    readonly property int maxDue: Math.max(1, ...dueCounts.slice(1))

    spacing: Theme.spacing.medium

    Repeater {
        model: 7

        Column {
            required property int index
            readonly property int offset: index - root.todayIndex
            readonly property string dayName: Qt.locale().dayName(index + 1, Locale.NarrowFormat)

            spacing: Theme.spacing.xSmall

            ActivityCell {
                id: cell
                objectName: "weekDot" + index

                property bool animationReady: false
                property string text: (filled ? "active" : "inactive")
                                      + (today ? " (today)" : "")

                anchors.horizontalCenter: parent.horizontalCenter
                width: root.cellSize
                height: root.cellSize
                future: offset > 0
                today: offset === 0
                count: future ? (root.dueCounts[offset] ?? 0) : (root.dailyStages[index] ?? 0)
                maxCount: root.maxDue

                Component.onCompleted: animationReady = true

                onHeatChanged: {
                    if (animationReady && today && heat > 0)
                    {
                        bounce.restart()
                    }
                }

                Behavior on color {
                    enabled: cell.animationReady
                    ColorAnimation { duration: Theme.activity.animationDuration }
                }

                Behavior on border.color {
                    enabled: cell.animationReady
                    ColorAnimation { duration: Theme.activity.animationDuration }
                }

                SequentialAnimation {
                    id: bounce

                    NumberAnimation {
                        target: cell
                        property: "scale"
                        to: 1.4
                        duration: 200
                        easing.type: Easing.OutQuad
                    }
                    NumberAnimation {
                        target: cell
                        property: "scale"
                        to: 1.0
                        duration: 400
                        easing.type: Easing.OutBounce
                    }
                }
            }

            Text {
                objectName: "weekDayLabel" + index
                anchors.horizontalCenter: parent.horizontalCenter
                text: dayName.charAt(0).toUpperCase() + dayName.slice(1)
                font.pixelSize: Theme.fontSize.xSmall
                font.bold: offset === 0
                color: offset === 0 ? Theme.colors.textPrimary : Theme.colors.textSecondary
            }
        }
    }
}
