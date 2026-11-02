import QtQuick

import Themed.Components

Item {
    id: root

    property string label: ""
    property string hint: ""
    property string value: label
    property url iconSource: ""
    property bool round: false
    property bool pill: false
    property Component face: null
    readonly property bool hasFace: face !== null
    readonly property bool hintBeside: pill
    property bool special: false
    property bool active: false
    property bool accented: false
    property bool upperCase: false
    property bool repeats: false
    property bool showsPreview: !special
    property var alternates: []
    property var swipes: []
    property real swipeArcStart: -90
    property real swipeArcSpan: 180
    property bool showsSwipeRing: false
    property bool alternatesInRing: false
    property var flicks: []
    readonly property bool flickable: flicks.length > 0
    readonly property var ringCharacters: swipeRing.held ? alternates : swipes
    readonly property real swipeStep: arcStep(swipes.length)
    readonly property real ringStep: arcStep(ringCharacters.length)
    readonly property bool ringShown: swipeRing.visible
    readonly property real circleRadius: Math.min(width, height) / 2
    readonly property real shapeRadius: round ? circleRadius : Math.hypot(width, height) / 2

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

    function arcStep(count) {
        return swipeArcSpan / (swipeArcSpan >= 360 ? count : Math.max(1, count - 1))
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

    function swipeIndexAt(x, y) {
        var angle = Math.atan2(y - root.height / 2, x - root.width / 2) * 180 / Math.PI
        var count = root.ringCharacters.length
        var nearest = 0
        var nearestDistance = 360
        for (var index = 0; index < count; ++index) {
            var center = root.swipeArcStart + index * root.ringStep
            var distance = Math.abs((((angle - center) % 360) + 540) % 360 - 180)
            if (distance < nearestDistance) {
                nearest = index
                nearestDistance = distance
            }
        }
        return nearest
    }

    function isOutside(x, y) {
        if (root.round)
            return Math.hypot(x - root.width / 2, y - root.height / 2) > root.circleRadius
        return x < 0 || y < 0 || x > root.width || y > root.height
    }

    implicitWidth: Theme.keyboard.keyHeight
    implicitHeight: Theme.keyboard.keyHeight
    z: area.pressed ? 1 : 0

    Rectangle {
        anchors.centerIn: parent
        width: root.round ? 2 * root.circleRadius : root.width
        height: root.round ? width : root.height
        radius: root.round || root.pill ? root.circleRadius : Theme.keyboard.keyRadius
        color: root.active ? Theme.keyboard.activeKeyBackground
             : area.pressed ? Theme.keyboard.keyPressedBackground
             : root.special ? Theme.keyboard.specialKeyBackground
             : Theme.keyboard.keyBackground
    }

    Text {
        id: labelText
        anchors.centerIn: parent
        anchors.verticalCenterOffset: root.hint === "" || root.hintBeside ? 0 : -hintText.height / 2
        anchors.horizontalCenterOffset: root.hint === "" || !root.hintBeside ? 0 : -hintText.width / 2
        visible: !root.hasFace && root.iconSource.toString() === ""
        text: root.displayed(root.label)
        font.pixelSize: root.special ? Theme.keyboard.specialFontSize : Theme.keyboard.fontSize
        color: root.contentColor
    }

    Text {
        id: hintText
        anchors.top: labelText.bottom
        anchors.topMargin: root.hintBeside ? labelText.baselineOffset - baselineOffset - labelText.height : 0
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.horizontalCenterOffset: root.hintBeside ? labelText.width / 2 : 0
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

    Rectangle {
        id: swipePreview
        objectName: root.objectName === "" ? "" : root.objectName + "Swipe"

        property int highlighted: -1
        readonly property string character: highlighted >= 0 ? root.ringCharacters[highlighted] : ""

        visible: highlighted >= 0 && !root.showsSwipeRing && !swipeRing.held
        anchors.bottom: parent.top
        anchors.bottomMargin: Theme.keyboard.keySpacing
        width: root.popupCellWidth
        height: root.popupHeight
        radius: Theme.keyboard.keyRadius
        color: Theme.keyboard.activeKeyBackground
        border.width: Theme.border.thin
        border.color: Theme.keyboard.previewBorder

        Text {
            anchors.centerIn: parent
            text: root.displayed(swipePreview.character)
            font.pixelSize: Math.round(Theme.keyboard.fontSize * Theme.keyboard.previewScale)
            color: Theme.keyboard.activeKeyText
        }
    }

    Item {
        id: swipeRing
        objectName: root.objectName === "" ? "" : root.objectName + "SwipeRing"

        readonly property real innerRadius: root.shapeRadius + Theme.keyboard.keySpacing
        readonly property real outerRadius: innerRadius + root.popupCellWidth
        readonly property real middleRadius: (innerRadius + outerRadius) / 2

        property bool held: false

        function sectorAngle(index) {
            return (root.swipeArcStart + index * root.ringStep) * Math.PI / 180
        }

        visible: held || (root.showsSwipeRing && swipePreview.highlighted >= 0)
        anchors.centerIn: parent
        width: 2 * outerRadius
        height: 2 * outerRadius

        onVisibleChanged: sectors.requestPaint()
        onHeldChanged: sectors.requestPaint()

        Connections {
            target: swipePreview

            function onHighlightedChanged() {
                sectors.requestPaint()
            }
        }

        Canvas {
            id: sectors
            anchors.fill: parent

            onPaint: {
                var context = getContext("2d")
                context.reset()
                var center = width / 2
                var halfStep = root.ringStep * Math.PI / 360
                context.lineWidth = Theme.border.thin
                context.strokeStyle = Theme.keyboard.previewBorder
                for (var index = 0; index < root.ringCharacters.length; ++index) {
                    var from = swipeRing.sectorAngle(index) - halfStep
                    var to = swipeRing.sectorAngle(index) + halfStep
                    context.beginPath()
                    context.arc(center, center, swipeRing.outerRadius, from, to, false)
                    context.arc(center, center, swipeRing.innerRadius, to, from, true)
                    context.closePath()
                    context.fillStyle = index === swipePreview.highlighted
                                        ? Theme.keyboard.activeKeyBackground
                                        : Theme.keyboard.previewBackground
                    context.fill()
                    context.stroke()
                }
            }
        }

        Repeater {
            model: root.showsSwipeRing || root.alternatesInRing ? root.ringCharacters : []

            Text {
                required property string modelData
                required property int index

                readonly property real angle: swipeRing.sectorAngle(index)
                readonly property bool highlighted: index === swipePreview.highlighted

                objectName: swipeRing.objectName === "" ? "" : swipeRing.objectName + "_" + index
                x: swipeRing.width / 2 + swipeRing.middleRadius * Math.cos(angle) - width / 2
                y: swipeRing.height / 2 + swipeRing.middleRadius * Math.sin(angle) - height / 2
                text: root.displayed(modelData)
                font.pixelSize: Theme.keyboard.fontSize
                color: highlighted ? Theme.keyboard.activeKeyText : Theme.keyboard.keyText
            }
        }
    }

    Item {
        id: cross
        objectName: root.objectName === "" ? "" : root.objectName + "Cross"

        readonly property real tileHeight: root.popupHeight
        readonly property real gap: Theme.keyboard.keySpacing
        readonly property real centerWidth: root.popupCellWidth
        readonly property real sideWidth: (root.width - gap) / 2
        readonly property real centerX: (root.width - centerWidth) / 2
        readonly property real middleY: tileHeight + gap
        readonly property var arms: [
            { name: "up", x: centerX, y: 0, width: centerWidth },
            { name: "right", x: root.width - sideWidth, y: middleY, width: sideWidth },
            { name: "down", x: centerX, y: 2 * middleY, width: centerWidth },
            { name: "left", x: 0, y: middleY, width: sideWidth }
        ]
        readonly property string character: direction >= 0 ? root.flicks[direction] ?? "" : ""

        property int direction: -1

        visible: false
        anchors.centerIn: parent
        width: root.width
        height: 3 * tileHeight + 2 * gap

        Repeater {
            model: root.flickable ? cross.arms : []

            Rectangle {
                required property var modelData
                required property int index

                readonly property string character: root.flicks[index] ?? ""
                readonly property bool highlighted: character !== "" && index === cross.direction

                objectName: cross.objectName === "" ? "" : cross.objectName + "_" + modelData.name
                x: modelData.x
                y: modelData.y
                width: modelData.width
                height: cross.tileHeight
                radius: Theme.keyboard.keyRadius
                color: highlighted ? Theme.keyboard.activeKeyBackground
                                   : Theme.keyboard.previewBackground
                border.width: Theme.border.thin
                border.color: Theme.keyboard.previewBorder

                Text {
                    anchors.centerIn: parent
                    text: root.displayed(parent.character)
                    font.pixelSize: Math.round(Theme.keyboard.fontSize * Theme.keyboard.previewScale)
                    color: parent.highlighted ? Theme.keyboard.activeKeyText
                                              : Theme.keyboard.keyText
                }
            }
        }
    }

    Timer {
        id: crossTimer
        interval: Theme.keyboard.flickCrossDelay
        onTriggered: cross.visible = true
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
        property bool flicking: false
        property point origin

        function highlightAlternateAt(x, y) {
            var point = area.mapToItem(alternatesRow, x, y)
            var index = Math.floor(point.x / root.popupCellWidth)
            alternatesPopup.highlighted = Math.max(0, Math.min(root.alternates.length - 1, index))
        }

        function endGesture() {
            crossTimer.stop()
            cross.visible = false
            cross.direction = -1
            swipeRing.held = false
            swipePreview.highlighted = -1
            flicking = false
        }

        anchors.fill: parent
        preventStealing: true
        pressAndHoldInterval: Theme.keyboard.repeatDelay

        onPressed: (mouse) => {
            area.endGesture()
            consumed = false
            felt = -1
            origin = Qt.point(mouse.x, mouse.y)
            KeyboardHaptics.keyPress()
            root.touched()
            if (root.flickable)
                crossTimer.restart()
        }

        onPressAndHold: (mouse) => {
            if (flicking || swipePreview.highlighted >= 0)
                return
            if (root.alternates.length > 0) {
                crossTimer.stop()
                cross.visible = false
                if (root.alternatesInRing) {
                    swipePreview.highlighted = -1
                    swipeRing.held = true
                    area.trackSwipe(mouse.x, mouse.y)
                } else {
                    alternatesPopup.x = root.popupX(root.alternates.length * root.popupCellWidth)
                    area.highlightAlternateAt(mouse.x, mouse.y)
                    alternatesPopup.visible = true
                }
                consumed = true
                KeyboardHaptics.longPress()
            } else if (root.repeats) {
                KeyboardHaptics.keyPress()
                root.activated(root.value)
                repeatTimer.start()
                consumed = true
            }
        }

        function trackSwipe(x, y) {
            if (!root.isOutside(x, y)) {
                swipePreview.highlighted = -1
                return
            }
            var index = root.swipeIndexAt(x, y)
            if (index === swipePreview.highlighted)
                return
            if (swipePreview.highlighted < 0)
                swipePreview.x = root.popupX(swipePreview.width)
            swipePreview.highlighted = index
            consumed = true
            if (index !== felt) {
                felt = index
                KeyboardHaptics.keyPress()
            }
        }

        function trackFlick(x, y) {
            var dx = x - origin.x
            var dy = y - origin.y
            var beyond = Math.hypot(dx, dy) > Theme.keyboard.flickThreshold
            if (!flicking) {
                if (!beyond)
                    return
                flicking = true
                consumed = true
                cross.visible = true
            }
            var direction = beyond ? root.flickDirectionAt(dx, dy) : -1
            if (direction === cross.direction)
                return
            cross.direction = direction
            if (direction >= 0 && direction !== felt) {
                felt = direction
                KeyboardHaptics.keyPress()
            }
        }

        onPositionChanged: (mouse) => {
            if (alternatesPopup.visible)
                area.highlightAlternateAt(mouse.x, mouse.y)
            else if (root.flickable)
                area.trackFlick(mouse.x, mouse.y)
            else if ((root.swipes.length > 0 || swipeRing.held) && !repeatTimer.running)
                area.trackSwipe(mouse.x, mouse.y)
        }

        onReleased: {
            repeatTimer.stop()
            var swiped = swipePreview.character
            var flicked = cross.character
            area.endGesture()
            if (alternatesPopup.visible) {
                alternatesPopup.visible = false
                root.activated(root.alternates[alternatesPopup.highlighted])
            } else if (swiped !== "") {
                root.activated(swiped)
            } else if (flicked !== "") {
                root.activated(flicked)
            } else if (!consumed && containsMouse) {
                root.activated(root.value)
            }
        }

        onCanceled: {
            repeatTimer.stop()
            alternatesPopup.visible = false
            area.endGesture()
        }
    }
}
