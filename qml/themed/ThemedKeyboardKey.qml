import QtQuick

import Themed.Components

Item {
    id: root

    property string label: ""
    property string value: label
    property url iconSource: ""
    property bool special: false
    property bool active: false
    property bool accented: false
    property bool upperCase: false
    property bool repeats: false
    property bool showsPreview: !special
    property var alternates: []

    readonly property bool pressed: area.pressed
    readonly property real popupCellWidth: width * Theme.keyboard.previewScale
    readonly property color contentColor: active ? Theme.keyboard.activeKeyText
                                        : accented ? Theme.keyboard.accentedKeyText
                                        : Theme.keyboard.keyText

    signal activated(string value)
    signal touched()

    function displayed(text) {
        return root.upperCase ? text.toUpperCase() : text
    }

    implicitWidth: Theme.keyboard.keyHeight
    implicitHeight: Theme.keyboard.keyHeight
    z: area.pressed ? 1 : 0

    Rectangle {
        anchors.fill: parent
        radius: Theme.keyboard.keyRadius
        color: root.active ? Theme.keyboard.activeKeyBackground
             : area.pressed ? Theme.keyboard.keyPressedBackground
             : root.special ? Theme.keyboard.specialKeyBackground
             : Theme.keyboard.keyBackground
    }

    Text {
        anchors.centerIn: parent
        visible: root.iconSource.toString() === ""
        text: root.displayed(root.label)
        font.pixelSize: root.special ? Theme.keyboard.specialFontSize : Theme.keyboard.fontSize
        color: root.contentColor
    }

    ThemedIcon {
        anchors.centerIn: parent
        visible: root.iconSource.toString() !== ""
        svgSource: root.iconSource
        color: root.contentColor
        width: Theme.keyboard.iconSize
        height: Theme.keyboard.iconSize
    }

    Rectangle {
        objectName: root.objectName === "" ? "" : root.objectName + "Preview"
        visible: root.showsPreview && area.pressed && !alternatesPopup.visible
        anchors.bottom: parent.top
        anchors.bottomMargin: Theme.keyboard.keySpacing
        anchors.horizontalCenter: parent.horizontalCenter
        width: root.popupCellWidth
        height: parent.height * Theme.keyboard.previewScale
        radius: Theme.keyboard.keyRadius
        color: Theme.keyboard.previewBackground
        border.width: Theme.border.thin
        border.color: Theme.keyboard.previewBorder

        Text {
            anchors.centerIn: parent
            text: root.displayed(root.label)
            font.pixelSize: Math.round(Theme.keyboard.fontSize * Theme.keyboard.previewScale)
            color: Theme.keyboard.keyText
        }
    }

    Rectangle {
        id: alternatesPopup
        objectName: root.objectName === "" ? "" : root.objectName + "Alternates"

        property int highlighted: 0

        visible: false
        anchors.bottom: parent.top
        anchors.bottomMargin: Theme.keyboard.keySpacing
        anchors.horizontalCenter: parent.horizontalCenter
        width: alternatesRow.width
        height: root.height * Theme.keyboard.previewScale
        radius: Theme.keyboard.keyRadius
        color: Theme.keyboard.previewBackground
        border.width: Theme.border.thin
        border.color: Theme.keyboard.previewBorder

        Row {
            id: alternatesRow
            anchors.verticalCenter: parent.verticalCenter

            Repeater {
                model: root.alternates

                Rectangle {
                    required property string modelData
                    required property int index

                    readonly property bool highlighted: index === alternatesPopup.highlighted

                    width: root.popupCellWidth
                    height: alternatesPopup.height
                    radius: Theme.keyboard.keyRadius
                    color: highlighted ? Theme.keyboard.activeKeyBackground : "transparent"

                    Text {
                        anchors.centerIn: parent
                        text: root.displayed(parent.modelData)
                        font.pixelSize: Theme.keyboard.fontSize
                        color: parent.highlighted ? Theme.keyboard.activeKeyText
                                                  : Theme.keyboard.keyText
                    }
                }
            }
        }
    }

    Timer {
        id: repeatTimer
        interval: Theme.keyboard.repeatInterval
        repeat: true
        onTriggered: {
            KeyboardHaptics.keyPress()
            root.activated(root.value)
        }
    }

    MouseArea {
        id: area

        property bool consumed: false

        anchors.fill: parent
        preventStealing: true
        pressAndHoldInterval: Theme.keyboard.repeatDelay

        onPressed: {
            consumed = false
            KeyboardHaptics.keyPress()
            root.touched()
        }

        onPressAndHold: {
            if (root.alternates.length > 0) {
                alternatesPopup.highlighted = 0
                alternatesPopup.visible = true
                consumed = true
                KeyboardHaptics.longPress()
            } else if (root.repeats) {
                KeyboardHaptics.keyPress()
                root.activated(root.value)
                repeatTimer.start()
                consumed = true
            }
        }

        onPositionChanged: (mouse) => {
            if (!alternatesPopup.visible)
                return
            var point = area.mapToItem(alternatesRow, mouse.x, mouse.y)
            var index = Math.floor(point.x / root.popupCellWidth)
            alternatesPopup.highlighted = Math.max(0, Math.min(root.alternates.length - 1, index))
        }

        onReleased: {
            repeatTimer.stop()
            if (alternatesPopup.visible) {
                alternatesPopup.visible = false
                root.activated(root.alternates[alternatesPopup.highlighted])
            } else if (!consumed && containsMouse) {
                root.activated(root.value)
            }
        }

        onCanceled: {
            repeatTimer.stop()
            alternatesPopup.visible = false
        }
    }
}
