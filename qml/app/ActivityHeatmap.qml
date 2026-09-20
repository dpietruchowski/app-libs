import QtQuick
import Themed.Components

Column {
    id: root

    property var levels: []
    property var forecast: []
    property int maxCount: 1
    property date startDate: new Date()
    property int weeks: 18
    property string pastLegend: qsTr("done")
    property string futureLegend: qsTr("planned")

    readonly property int cellSize: Theme.activity.cellSize
    readonly property int todayIndex: levels.length - 1

    function dateAt(index) {
        const date = new Date(startDate)
        date.setDate(date.getDate() + index)
        return date
    }

    function countAt(index) {
        if (index < levels.length) {
            return levels[index]
        }
        const ahead = index - levels.length
        return ahead < forecast.length ? forecast[ahead] : 0
    }

    spacing: Theme.spacing.small

    Row {
        objectName: "heatmapMonths"
        spacing: Theme.spacing.xSmall

        Repeater {
            model: root.weeks

            Text {
                readonly property date columnStart: root.dateAt(index * 7)
                readonly property bool startsMonth: index === 0
                    || columnStart.getMonth() !== root.dateAt((index - 1) * 7).getMonth()

                width: root.cellSize
                text: startsMonth
                    ? Qt.locale().standaloneMonthName(columnStart.getMonth() + 1,
                                                      Locale.ShortFormat)
                    : ""
                font.pixelSize: Theme.fontSize.xSmall
                color: Theme.colors.textSecondary
            }
        }
    }

    Row {
        objectName: "heatmapGrid"
        spacing: Theme.spacing.xSmall

        Repeater {
            model: root.weeks

            Column {
                readonly property int week: index

                spacing: Theme.spacing.xSmall

                Repeater {
                    model: 7

                    ActivityCell {
                        readonly property int cell: parent.week * 7 + index

                        objectName: "heatmapCell" + cell
                        width: root.cellSize
                        height: root.cellSize
                        radius: Theme.radius.small
                        count: root.countAt(cell)
                        maxCount: root.maxCount
                        future: cell >= root.levels.length
                        today: cell === root.todayIndex
                    }
                }
            }
        }
    }

    Row {
        objectName: "heatmapLegend"
        spacing: Theme.spacing.medium

        component LegendEntry: Row {
            property alias future: swatch.future
            property string label: ""

            spacing: Theme.spacing.xSmall

            ActivityCell {
                id: swatch
                width: root.cellSize
                height: root.cellSize
                radius: Theme.radius.small
                count: 1
                maxCount: 1
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                text: parent.label
                font.pixelSize: Theme.fontSize.xSmall
                color: Theme.colors.textSecondary
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        LegendEntry {
            objectName: "heatmapPastLegend"
            future: false
            label: root.pastLegend
        }

        LegendEntry {
            objectName: "heatmapFutureLegend"
            future: true
            label: root.futureLegend
        }
    }
}
