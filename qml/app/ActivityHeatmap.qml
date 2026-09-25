import QtQuick
import Themed.Components

ThemedCard {
    id: root

    property var dailyStages: []
    property var dueCounts: []
    property date today: new Date()
    property int pastWeeks: 14
    property int futureWeeks: 4
    property int streakDays: 0
    property string title: qsTr("Activity")
    property string pastLegend: qsTr("Daily stage")
    property string futureLegend: qsTr("Reviews due")
    property string futureMarker: qsTr("scheduled →")
    property string dueTodayLabel: qsTr("Due today")
    property string dueNextWeekLabel: qsTr("Next 7 days")
    property string streakLabel: qsTr("Day streak")

    readonly property int dueToday: dueCounts.length > 0 ? dueCounts[0] : 0
    readonly property int dueNextWeek: dueCounts.slice(1, 8).reduce((sum, value) => sum + value, 0)
    readonly property int columns: pastWeeks + futureWeeks
    readonly property real innerWidth: width - Theme.padding.medium * 2
    readonly property real cellGap: Theme.scaled(3)
    readonly property real cellSize: Math.floor((innerWidth - cellGap * (columns - 1)) / columns)
    readonly property real gridWidth: cellSize * columns + cellGap * (columns - 1)
    readonly property date firstDay: {
        const day = new Date(today.getFullYear(), today.getMonth(), today.getDate())
        const weekday = (day.getDay() + 6) % 7
        day.setDate(day.getDate() - weekday - (pastWeeks - 1) * 7)
        return day
    }
    readonly property int todayIndex: Math.round(
        (new Date(today.getFullYear(), today.getMonth(), today.getDate()) - firstDay) / 86400000)
    readonly property int maxDue: Math.max(1, ...dueCounts.slice(1))

    function dayAt(index) {
        const day = new Date(root.firstDay)
        day.setDate(day.getDate() + index)
        return day
    }

    function monthLabel(column) {
        const monday = root.dayAt(column * 7)
        if (column > 0 && root.dayAt((column - 1) * 7).getMonth() === monday.getMonth())
        {
            return ""
        }
        return Qt.locale(Qt.uiLanguage).standaloneMonthName(monday.getMonth(), Locale.ShortFormat)
    }

    component Swatch: ActivityCell {
        width: root.cellSize
        height: root.cellSize
    }

    component Stat: Column {
        id: stat

        property string value: ""
        property string label: ""

        spacing: Theme.spacing.xSmall

        Text {
            text: stat.value
            color: Theme.colors.textPrimary
            font.pixelSize: Theme.fontSize.xLarge
            font.bold: true
        }

        ThemedText {
            textStyle: Theme.text.caption
            width: parent.width
            text: stat.label
            wrapMode: Text.Wrap
        }
    }

    component LegendLabel: ThemedText {
        textStyle: Theme.text.caption
        anchors.verticalCenter: parent.verticalCenter
        rightPadding: Theme.spacing.xSmall
    }

    content: Item {
        implicitHeight: heatmapColumn.height + Theme.padding.medium * 2

        Column {
            id: heatmapColumn
            x: Theme.padding.medium
            y: Theme.padding.medium
            width: root.innerWidth
            spacing: Theme.spacing.medium

            Item {
                width: parent.width
                height: titleText.height

                ThemedText {
                    id: titleText
                    objectName: "activityTitle"
                    textStyle: Theme.text.title
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.title
                }

                ThemedText {
                    objectName: "activityRange"
                    textStyle: Theme.text.caption
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("last %n week(s)", "", root.pastWeeks)
                }
            }

            Column {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.spacing.xSmall

                Grid {
                    objectName: "activityHeatmapGrid"
                    rows: 7
                    flow: Grid.TopToBottom
                    spacing: root.cellGap

                    Repeater {
                        model: root.columns * 7

                        Swatch {
                            required property int index
                            readonly property int offset: index - root.todayIndex

                            future: offset > 0
                            today: offset === 0
                            count: future
                                   ? (root.dueCounts[offset] ?? 0)
                                   : (root.dailyStages[-offset] ?? 0)
                            maxCount: root.maxDue
                        }
                    }
                }

                Item {
                    width: root.gridWidth
                    height: scheduledText.height

                    Repeater {
                        model: root.columns

                        ThemedText {
                            required property int index
                            textStyle: Theme.text.caption
                            x: index * (root.cellSize + root.cellGap)
                            text: root.monthLabel(index)
                            visible: x + implicitWidth + Theme.spacing.small <= scheduledText.x
                        }
                    }

                    Text {
                        id: scheduledText
                        objectName: "activityScheduledLabel"
                        anchors.right: parent.right
                        text: root.futureMarker
                        color: Theme.activity.scheduled
                        font.pixelSize: Theme.fontSize.small
                        font.bold: true
                    }
                }
            }

            Flow {
                width: parent.width
                spacing: Theme.spacing.large

                Row {
                    spacing: Theme.spacing.xSmall

                    LegendLabel {
                        objectName: "activityStageLegend"
                        text: root.pastLegend
                    }

                    Repeater {
                        model: [1, 3, 5, 8]

                        Swatch {
                            required property int modelData
                            anchors.verticalCenter: parent.verticalCenter
                            count: modelData
                        }
                    }
                }

                Row {
                    spacing: Theme.spacing.xSmall

                    LegendLabel {
                        objectName: "activityDueLegend"
                        text: root.futureLegend
                    }

                    Repeater {
                        model: 4

                        Swatch {
                            required property int index
                            anchors.verticalCenter: parent.verticalCenter
                            future: true
                            count: index + 1
                            maxCount: 4
                        }
                    }
                }
            }

            ThemedSeparator {
                width: parent.width
                color: Theme.colors.border
            }

            Row {
                id: stats

                readonly property real separatorWidth: Theme.separator.thickness
                readonly property real statWidth: (width - separatorWidth * 2 - spacing * 4) / 3

                width: parent.width
                height: Math.max(dueTodayStat.implicitHeight, dueNextWeekStat.implicitHeight,
                                 streakStat.implicitHeight)
                spacing: Theme.spacing.medium

                Stat {
                    id: dueTodayStat
                    objectName: "activityDueToday"
                    width: stats.statWidth
                    value: root.dueToday
                    label: root.dueTodayLabel
                }

                ThemedSeparator {
                    vertical: true
                    height: parent.height
                    color: Theme.colors.border
                }

                Stat {
                    id: dueNextWeekStat
                    objectName: "activityDueNextWeek"
                    width: stats.statWidth
                    value: root.dueNextWeek
                    label: root.dueNextWeekLabel
                }

                ThemedSeparator {
                    vertical: true
                    height: parent.height
                    color: Theme.colors.border
                }

                Stat {
                    id: streakStat
                    objectName: "activityStreak"
                    width: stats.statWidth
                    value: root.streakDays
                    label: root.streakLabel
                }
            }
        }
    }
}
