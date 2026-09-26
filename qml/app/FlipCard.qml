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
    property bool holdTextUntilFlip: false
    property string shownSourceText: ""
    property string shownTargetText: ""
    readonly property bool textPending: holdTextUntilFlip
                                        && (shownSourceText !== sourceText || shownTargetText !== targetText)
    readonly property string displayedSourceText: holdTextUntilFlip ? shownSourceText : sourceText
    readonly property string displayedTargetText: holdTextUntilFlip ? shownTargetText : targetText
    property string text: flipped ? displayedTargetText : displayedSourceText
    property int lineCount: 0

    function showCurrentText() {
        shownSourceText = sourceText
        shownTargetText = targetText
    }

    onSourceTextChanged: if (shownSourceText === "") showCurrentText()
    onTargetTextChanged: if (shownTargetText === "") showCurrentText()
    Component.onCompleted: showCurrentText()

    implicitWidth: card.implicitWidth
    implicitHeight: lineCount > 0 ? Math.ceil(fontMetrics.lineSpacing * lineCount) + 1 + card.margins * 2
                                  : card.implicitHeight

    function flip() {
        if (root.enabled && !spinAnimation.running) {
            flipAnimation.start()
        }
    }

    function spin() {
        if (root.enabled && !flipAnimation.running) {
            spinAnimation.start()
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
                text: root.text
                font.pixelSize: root.textSize
                font.bold: true
                color: root.enabled ? Theme.colors.textPrimary : Theme.colors.textDisabled
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
                maximumLineCount: root.lineCount > 0 ? root.lineCount
                                                     : Math.max(1, Math.floor(height / fontMetrics.height))
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
            script: {
                root.showCurrentText()
                root.flipped = !root.flipped
            }
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

    SequentialAnimation {
        id: spinAnimation
        NumberAnimation {
            target: rotation
            property: "angle"
            to: rotation.angle < 90 ? 90 : 270
            duration: 100
            easing.type: Easing.InQuad
        }
        ScriptAction {
            script: {
                root.showCurrentText()
                rotation.angle = rotation.angle < 180 ? 270 : 90
            }
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
