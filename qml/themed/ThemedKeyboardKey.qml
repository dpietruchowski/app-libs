import QtQuick

import Themed.Components

Item {
    id: root

    property string label: ""
    property string hint: ""
    property string value: label
    property url iconSource: ""
    property Component face: null
    readonly property bool hasFace: face !== null
    property bool special: false
    property bool active: false
    property bool accented: false
    property bool upperCase: false
    property bool repeats: false
    property bool showsPreview: !special
    property var alternates: []
    property var flicks: []
    readonly property bool flickable: flicks.length > 0
    readonly property real circleRadius: Math.min(width, height) / 2

    property real popupCellWidth: width * Theme.keyboard.previewScale
    property real popupHeight: height * Theme.keyboard.previewScale

    readonly property bool pressed: area.pressed
    readonly property color contentColor: active ? Theme.keyboard.activeKeyText
                                        : accented ? Theme.keyboard.accentedKeyText
                                        : Theme.keyboard.keyText
    readonly property color hintColor: active ? contentColor : Theme.keyboard.hintText

    signal activated(string value)
    signal touched()

    function displayed(text) {
        return root.upperCase ? text.toUpperCase() : text
    }

    function windowShift(start, size, windowSize) {
        var margin = Theme.keyboard.keySpacing
        var overflowEnd = start + size - (windowSize - margin)
        var overflowStart = margin - start
        return overflowStart > 0 ? overflowStart : overflowEnd > 0 ? -overflowEnd : 0
    }

    function popupX(popupWidth) {
        var centered = (root.width - popupWidth) / 2
        return centered + root.windowShift(root.mapToItem(null, centered, 0).x, popupWidth,
                                           root.Window.width)
    }

    function flickDirectionAt(dx, dy) {
        return (Math.round(Math.atan2(dy, dx) / (Math.PI / 2)) + 5) % 4
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
        id: labelText
        anchors.centerIn: parent
        anchors.verticalCenterOffset: root.hint === "" ? 0 : -hintText.height / 2
        visible: !root.hasFace && root.iconSource.toString() === ""
        text: root.displayed(root.label)
        font.pixelSize: root.special ? Theme.keyboard.specialFontSize : Theme.keyboard.fontSize
        color: root.contentColor
    }

    Text {
        id: hintText
        anchors.top: labelText.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        visible: !root.hasFace && root.hint !== ""
        text: root.displayed(root.hint)
        font.pixelSize: Theme.keyboard.hintFontSize
        color: root.hintColor
    }

    ThemedIcon {
        anchors.centerIn: parent
        visible: !root.hasFace && root.iconSource.toString() !== ""
        svgSource: root.iconSource
        color: root.contentColor
        width: Theme.keyboard.iconSize
        height: Theme.keyboard.iconSize
    }

    Loader {
        anchors.fill: parent
        sourceComponent: root.face
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
        width: alternatesRow.width
        height: root.popupHeight
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

    Item {
        id: flickPad
        objectName: root.objectName === "" ? "" : root.objectName + "FlickPad"

        readonly property var directions: [Qt.point(0, -1), Qt.point(1, 0), Qt.point(0, 1), Qt.point(-1, 0)]
        readonly property real letterDistance: width * 0.36
        readonly property string character: direction >= 0 ? root.flicks[direction] ?? "" : ""

        property int direction: -1

        visible: direction >= 0
        anchors.centerIn: parent
        width: Math.max(root.width, root.height) * Theme.keyboard.previewScale
        height: width

        onVisibleChanged: sectors.requestPaint()
        onCharacterChanged: sectors.requestPaint()

        Canvas {
            id: sectors
            anchors.fill: parent

            onPaint: {
                var context = getContext("2d")
                context.reset()
                var side = width
                var center = side / 2
                var corners = [[0, 0], [side, 0], [side, side], [0, side]]
                context.save()
                context.beginPath()
                context.roundedRect(0, 0, side, side, Theme.keyboard.keyRadius, Theme.keyboard.keyRadius)
                context.clip()
                context.fillStyle = Theme.keyboard.previewBackground
                context.fillRect(0, 0, side, side)
                if (flickPad.character !== "") {
                    var from = corners[flickPad.direction]
                    var to = corners[(flickPad.direction + 1) % 4]
                    context.beginPath()
                    context.moveTo(center, center)
                    context.lineTo(from[0], from[1])
                    context.lineTo(to[0], to[1])
                    context.closePath()
                    context.fillStyle = Theme.keyboard.activeKeyBackground
                    context.fill()
                }
                context.restore()
                context.lineWidth = Theme.border.medium
                context.strokeStyle = Theme.keyboard.background
                context.beginPath()
                context.moveTo(0, 0)
                context.lineTo(side, side)
                context.moveTo(side, 0)
                context.lineTo(0, side)
                context.stroke()
                context.beginPath()
                context.roundedRect(0, 0, side, side, Theme.keyboard.keyRadius, Theme.keyboard.keyRadius)
                context.stroke()
                context.beginPath()
                context.arc(center, center, root.circleRadius, 0, 2 * Math.PI, false)
                context.fillStyle = Theme.keyboard.keyBackground
                context.fill()
                context.stroke()
            }
        }

        Repeater {
            model: root.flickable ? root.flicks : []

            Text {
                required property string modelData
                required property int index

                readonly property bool highlighted: modelData !== "" && index === flickPad.direction

                objectName: flickPad.objectName === "" ? "" : flickPad.objectName + "_" + index
                x: flickPad.width / 2 + flickPad.letterDistance * flickPad.directions[index].x - width / 2
                y: flickPad.height / 2 + flickPad.letterDistance * flickPad.directions[index].y - height / 2
                text: root.displayed(modelData)
                font.pixelSize: Theme.keyboard.fontSize
                color: highlighted ? Theme.keyboard.activeKeyText : Theme.keyboard.keyText
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
        property int felt: -1

        function highlightAlternateAt(x, y) {
            var point = area.mapToItem(alternatesRow, x, y)
            var index = Math.floor(point.x / root.popupCellWidth)
            alternatesPopup.highlighted = Math.max(0, Math.min(root.alternates.length - 1, index))
        }

        function trackFlick(x, y) {
            var dx = x - root.width / 2
            var dy = y - root.height / 2
            var direction = Math.hypot(dx, dy) > root.circleRadius ? root.flickDirectionAt(dx, dy) : -1
            if (direction === flickPad.direction)
                return
            flickPad.direction = direction
            if (direction < 0)
                return
            consumed = true
            if (direction !== felt) {
                felt = direction
                KeyboardHaptics.keyPress()
            }
        }

        anchors.fill: parent
        preventStealing: true
        pressAndHoldInterval: Theme.keyboard.repeatDelay

        onPressed: {
            flickPad.direction = -1
            consumed = false
            felt = -1
            KeyboardHaptics.keyPress()
            root.touched()
        }

        onPressAndHold: (mouse) => {
            if (consumed)
                return
            if (root.alternates.length > 0) {
                alternatesPopup.x = root.popupX(root.alternates.length * root.popupCellWidth)
                area.highlightAlternateAt(mouse.x, mouse.y)
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
            if (alternatesPopup.visible)
                area.highlightAlternateAt(mouse.x, mouse.y)
            else if (root.flickable && !repeatTimer.running)
                area.trackFlick(mouse.x, mouse.y)
        }

        onReleased: {
            repeatTimer.stop()
            var flicked = flickPad.character
            flickPad.direction = -1
            if (alternatesPopup.visible) {
                alternatesPopup.visible = false
                root.activated(root.alternates[alternatesPopup.highlighted])
            } else if (flicked !== "") {
                root.activated(flicked)
            } else if (!consumed && containsMouse) {
                root.activated(root.value)
            }
        }

        onCanceled: {
            repeatTimer.stop()
            alternatesPopup.visible = false
            flickPad.direction = -1
        }
    }
}
