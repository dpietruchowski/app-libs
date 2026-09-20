import QtQuick
import Themed.Components

Row {
    id: root

    property var activity: []
    property var counts: []
    property var futureCounts: []
    property int maxCount: 1
    property int todayIndex: -1
    property bool clickable: false

    signal clicked()

    readonly property var levels: counts.length > 0
        ? counts
        : activity.map(day => day === true ? 1 : 0)

    function levelAt(index) {
        if (todayIndex >= 0 && index > todayIndex) {
            const ahead = index - todayIndex - 1
            return ahead < futureCounts.length ? futureCounts[ahead] : 0
        }
        return index < levels.length ? levels[index] : 0
    }

    spacing: Theme.spacing.large

    TapHandler {
        enabled: root.clickable
        onTapped: root.clicked()
    }

    Repeater {
        model: 7

        Column {
            spacing: Theme.spacing.xSmall

            ActivityCell {
                id: dot
                objectName: "weekDot" + index

                property bool animationReady: false
                property string text: (filled ? "active" : "inactive")
                    + (today ? " (today)" : "")

                width: Theme.activity.cellSize
                height: Theme.activity.cellSize
                radius: width / 2
                anchors.horizontalCenter: parent.horizontalCenter
                count: root.levelAt(index)
                maxCount: root.maxCount
                future: root.todayIndex >= 0 && index > root.todayIndex
                today: index === root.todayIndex

                Component.onCompleted: animationReady = true

                onFilledChanged: {
                    if (animationReady && filled) {
                        fillAnimation.restart()
                    }
                }

                Behavior on color {
                    enabled: dot.animationReady
                    ColorAnimation { duration: Theme.activity.animationDuration }
                }

                Behavior on border.color {
                    enabled: dot.animationReady
                    ColorAnimation { duration: Theme.activity.animationDuration }
                }

                SequentialAnimation {
                    id: fillAnimation

                    NumberAnimation {
                        target: dot
                        property: "scale"
                        to: 1.8
                        duration: 200
                        easing.type: Easing.OutQuad
                    }
                    NumberAnimation {
                        target: dot
                        property: "scale"
                        to: 1.0
                        duration: 400
                        easing.type: Easing.OutBounce
                    }
                }
            }

            Text {
                objectName: "weekDayLabel" + index
                text: Qt.locale().dayName(index + 1, Locale.NarrowFormat)
                font.pixelSize: Theme.fontSize.xSmall
                font.bold: index === root.todayIndex
                color: index === root.todayIndex
                    ? Theme.colors.textPrimary
                    : Theme.colors.textSecondary
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }
    }
}
