import QtQuick
import Themed.Components

Row {
    id: root

    property var activity: []
    property int todayIndex: -1

    spacing: Theme.spacing.large

    Repeater {
        model: 7

        Column {
            spacing: Theme.spacing.xSmall

            Rectangle {
                id: dot
                objectName: "weekDot" + index

                readonly property bool filled: root.activity[index] === true
                property string text: (filled ? "active" : "inactive") + (index === root.todayIndex ? " (today)" : "")
                property bool animationReady: false

                width: 12
                height: 12
                radius: 6
                anchors.horizontalCenter: parent.horizontalCenter
                color: filled ? Theme.colors.success : "transparent"
                border.color: filled ? Theme.colors.success : Theme.colors.textSecondary
                border.width: Theme.border.thin

                Component.onCompleted: animationReady = true

                onFilledChanged: {
                    if (animationReady && filled) {
                        fillAnimation.restart()
                    }
                }

                Behavior on color {
                    enabled: dot.animationReady
                    ColorAnimation { duration: 250 }
                }

                Behavior on border.color {
                    enabled: dot.animationReady
                    ColorAnimation { duration: 250 }
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
