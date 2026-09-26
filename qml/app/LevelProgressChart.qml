import QtQuick
import Themed.Components

Item {
    id: root

    implicitHeight: chartColumn.height

    property int learningCount: 0
    property var reviewCounts: [0, 0, 0, 0, 0, 0]
    property int masteredCount: 0
    property string title: qsTr("Progress by level")
    property string learningLabel: qsTr("Learning")
    property string masteredLabel: qsTr("Mastered")
    property string totalLabel: qsTr("%n item(s)", "", root.total)

    readonly property int reviewTotal: reviewCounts.reduce((sum, value) => sum + value, 0)
    readonly property int total: learningCount + reviewTotal + masteredCount
    readonly property int maxValue: Math.max(1, learningCount, masteredCount, ...reviewCounts)
    readonly property real innerWidth: width - Theme.padding.medium * 2
    readonly property real barAreaHeight: Math.max(Theme.scaled(100), innerWidth * 0.4)
    readonly property real groupSpacing: Theme.spacing.large
    readonly property real reviewSpacing: Theme.scaled(3)
    readonly property int reviewLevels: Math.max(1, reviewCounts.length)
    readonly property real barWidth: Math.max(0, Math.floor((innerWidth - groupSpacing * 2
                                                             - reviewSpacing * (reviewLevels - 1))
                                                            / (reviewLevels + 2.8)))
    readonly property real reviewGroupWidth: barWidth * reviewLevels + reviewSpacing * (reviewLevels - 1)
    readonly property real slotWidth: (innerWidth - groupSpacing * 2 - reviewGroupWidth) / 2

    function barHeight(value) {
        if (value <= 0)
            return 2
        return Math.max(Theme.spacing.small, root.barAreaHeight * Math.sqrt(value / root.maxValue))
    }

    component Bar: Item {
        id: bar

        property int value: 0
        property color barColor: Theme.phase.review
        property string label: ""

        height: root.barAreaHeight + valueText.height + labelText.height + Theme.spacing.small * 2

        Text {
            id: labelText
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            text: bar.label
            color: Theme.colors.textSecondary
            font.pixelSize: Theme.fontSize.small
        }

        Rectangle {
            id: fill
            anchors.bottom: labelText.top
            anchors.bottomMargin: Theme.spacing.small
            anchors.horizontalCenter: parent.horizontalCenter
            width: root.barWidth
            height: root.barHeight(bar.value)
            radius: Theme.radius.small
            color: bar.barColor

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: Math.min(parent.height, parent.radius)
                color: parent.color
            }

            Behavior on height {
                NumberAnimation {
                    duration: 300
                    easing.type: Easing.OutCubic
                }
            }
        }

        Text {
            id: valueText
            anchors.bottom: fill.top
            anchors.bottomMargin: Theme.spacing.xSmall
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: bar.value
            color: Theme.colors.textPrimary
            font.pixelSize: Theme.fontSize.small
            font.bold: true
        }
    }

    Column {
        id: chartColumn
        x: Theme.padding.medium
        width: root.innerWidth
        spacing: Theme.spacing.medium

        Item {
            width: parent.width
            height: titleText.height

            ThemedText {
                id: titleText
                objectName: "levelChartTitle"
                textStyle: Theme.text.title
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: root.title
            }

            ThemedText {
                objectName: "levelChartTotal"
                textStyle: Theme.text.caption
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: root.totalLabel
            }
        }

        Row {
            spacing: root.groupSpacing

            Bar {
                objectName: "levelChartLearning"
                width: root.slotWidth
                value: root.learningCount
                barColor: Theme.phase.untouched
                label: root.learningLabel
            }

            Column {
                width: root.reviewGroupWidth
                spacing: Theme.spacing.small

                Row {
                    spacing: root.reviewSpacing

                    Repeater {
                        model: root.reviewCounts

                        Bar {
                            required property int index
                            required property int modelData

                            objectName: "levelChartReview" + index
                            width: root.barWidth
                            value: modelData
                            barColor: Theme.phase.review
                            label: "L" + index
                        }
                    }
                }

                Row {
                    id: reviewTotalRow
                    width: parent.width
                    spacing: Theme.spacing.small

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: (parent.width - reviewTotalText.width) / 2 - reviewTotalRow.spacing
                        height: Theme.separator.thickness
                        color: Theme.colors.border
                    }

                    Text {
                        id: reviewTotalText
                        objectName: "levelChartReviewTotal"
                        text: qsTr("Review %1").arg(root.reviewTotal)
                        color: Theme.phase.review
                        font.pixelSize: Theme.fontSize.small
                        font.bold: true
                    }

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: (parent.width - reviewTotalText.width) / 2 - reviewTotalRow.spacing
                        height: Theme.separator.thickness
                        color: Theme.colors.border
                    }
                }
            }

            Bar {
                objectName: "levelChartMastered"
                width: root.slotWidth
                value: root.masteredCount
                barColor: Theme.phase.mastered
                label: root.masteredLabel
            }
        }
    }
}
