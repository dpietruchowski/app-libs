import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Themed.Components

Item {
    id: root

    property string sourceText: ""
    property string targetText: ""
    property bool flipped: false
    property bool enabled: true
    property real textSize: Theme.fontSize.xxLarge
    property string text: flipped ? targetText : sourceText

    implicitWidth: card.implicitWidth
    implicitHeight: card.implicitHeight

    function flip() {
        if (root.enabled) {
            flipAnimation.start()
        }
    }

    ThemedCard {
        id: card
        width: parent.width
        height: parent.height
        clickable: root.enabled
        opacity: root.enabled ? 1.0 : 0.5
        margins: root.textSize < Theme.fontSize.large ? Theme.spacing.small : Theme.spacing.large

        onClicked: {
            flip()
        }

        transform: Rotation {
            id: rotation
            origin.x: card.width / 2
            origin.y: card.height / 2
            axis { x: 0; y: 1; z: 0 }
            angle: 0
        }

        Item {
            clip: true

            Text {
                id: text
                anchors.fill: parent
                text: root.flipped ? root.targetText : root.sourceText
                font.pixelSize: root.textSize
                font.bold: true
                color: root.enabled ? Theme.colors.textPrimary : Theme.colors.textDisabled
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
                maximumLineCount: Math.max(1, Math.floor(height / fontMetrics.height))
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter

                transform: Scale {
                    origin.x: text.width / 2
                    xScale: rotation.angle > 90 && rotation.angle < 270 ? -1 : 1
                }
            }

            FontMetrics {
                id: fontMetrics
                font: text.font
            }
        }
    }

    SequentialAnimation {
        id: flipAnimation
        NumberAnimation {
            target: rotation
            property: "angle"
            to: rotation.angle < 90 ? 90 : 270
            duration: 100
            easing.type: Easing.InQuad
        }
        ScriptAction {
            script: root.flipped = !root.flipped
        }
        NumberAnimation {
            target: rotation
            property: "angle"
            to: rotation.angle < 180 ? 180 : 360
            duration: 100
            easing.type: Easing.OutQuad
        }
        ScriptAction {
            script: {
                if (rotation.angle >= 360) {
                    rotation.angle = 0
                }
            }
        }
    }
}
