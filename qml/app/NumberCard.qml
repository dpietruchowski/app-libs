import QtQuick
import QtQuick.Controls
import Themed.Components

ThemedCard {
    id: root

    property string title: "Title"
    property string mainText: ""
    property string mainLabel: ""
    property string subText: ""
    property string subLabel: ""
    property string backTitle: ""
    property string backMainText: ""
    property string backMainLabel: ""
    property bool flipped: false

    readonly property bool flippable: backMainText !== ""
    readonly property real flipAngle: rotation.angle
    readonly property string shownTitle: flipped ? backTitle : title
    readonly property string shownMainText: flipped ? backMainText : mainText
    readonly property string shownMainLabel: flipped ? backMainLabel : mainLabel

    function flip() {
        if (flippable && !flipAnimation.running)
            flipAnimation.start()
    }

    clickable: true
    backgroundColor: Theme.colors.surface

    onClicked: flip()

    transform: Rotation {
        id: rotation
        origin.x: root.width / 2
        origin.y: root.height / 2
        axis { x: 0; y: 1; z: 0 }
        angle: 0
    }

    readonly property SequentialAnimation flipAnimation: SequentialAnimation {

        NumberAnimation {
            target: rotation
            property: "angle"
            to: 90
            duration: 100
            easing.type: Easing.InQuad
        }
        ScriptAction {
            script: {
                root.flipped = !root.flipped
                rotation.angle = -90
            }
        }
        NumberAnimation {
            target: rotation
            property: "angle"
            to: 0
            duration: 100
            easing.type: Easing.OutQuad
        }
    }

    content: Item {
        Column {
            anchors.centerIn: parent
            spacing: Theme.spacing.small

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.shownTitle
                font.pixelSize: Theme.fontSize.xSmall
                color: Theme.colors.textSecondary
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.spacing.small

                Text {
                    id: mainValue
                    text: root.shownMainText
                    font.pixelSize: Theme.fontSize.xxLarge
                    font.bold: true
                    color: Theme.colors.textPrimary
                }

                Text {
                    visible: root.shownMainLabel !== ""
                    anchors.verticalCenter: mainValue.verticalCenter
                    text: root.shownMainLabel
                    font.pixelSize: Theme.fontSize.small
                    color: Theme.colors.textSecondary
                }
            }

            Row {
                visible: !root.flipped && (root.subText !== "" || root.subLabel !== "")
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.spacing.xSmall

                Text {
                    id: subValue
                    text: root.subText
                    font.pixelSize: Theme.fontSize.medium
                    font.bold: true
                    color: Theme.colors.textSecondary
                }

                Text {
                    visible: root.subLabel !== ""
                    anchors.baseline: subValue.baseline
                    text: root.subLabel
                    font.pixelSize: Theme.fontSize.small
                    color: Theme.colors.textSecondary
                }
            }
        }
    }
}
