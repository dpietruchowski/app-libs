import QtQuick

import Themed.Components

Item {
    id: root

    property string label: ""
    property string hint: ""
    property string value: label
    property url iconSource: ""
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
    readonly property real swipeStep: swipeArcSpan / (swipeArcSpan >= 360 ? swipes.length
                                                                          : Math.max(1, swipes.length - 1))

    property real popupCellWidth: width * Theme.keyboard.previewScale
    property real popupHeight: height * Theme.keyboard.previewScale

    readonly property bool pressed: area.pressed
    readonly property color contentColor: active ? Theme.keyboard.activeKeyText
                                        : accented ? Theme.keyboard.accentedKeyText
                                        : Theme.keyboard.keyText

    signal activated(string value)
    signal touched()

    function displayed(text) {
        return root.upperCase ? text.toUpperCase() : text
    }

    function popupX(popupWidth) {
        var centered = (root.width - popupWidth) / 2
        var left = root.mapToItem(null, centered, 0).x
        var margin = Theme.keyboard.keySpacing
        var overflowRight = left + popupWidth - (root.Window.width - margin)
        var overflowLeft = margin - left
        return centered + (overflowLeft > 0 ? overflowLeft : overflowRight > 0 ? -overflowRight : 0)
    }

    function swipeIndexAt(x, y) {
        var angle = Math.atan2(y - root.height / 2, x - root.width / 2) * 180 / Math.PI
        var count = root.swipes.length
        var nearest = 0
        var nearestDistance = 360
        for (var index = 0; index < count; ++index) {
            var center = root.swipeArcStart + index * root.swipeStep
            var distance = Math.abs((((angle - center) % 360) + 540) % 360 - 180)
            if (distance < nearestDistance) {
                nearest = index
                nearestDistance = distance
            }
        }
        return nearest
    }

    function isOutside(x, y) {
        return x < 0 || y < 0 || x > root.width || y > root.height
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
        visible: root.iconSource.toString() === ""
        text: root.displayed(root.label)
        font.pixelSize: root.special ? Theme.keyboard.specialFontSize : Theme.keyboard.fontSize
        color: root.contentColor
    }

    Text {
        id: hintText
        anchors.top: labelText.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        visible: root.hint !== ""
        text: root.displayed(root.hint)
        font.pixelSize: Theme.keyboard.hintFontSize
        color: root.active ? root.contentColor : Theme.keyboard.hintText
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
        readonly property string character: highlighted >= 0 ? root.swipes[highlighted] : ""

        visible: highlighted >= 0 && !root.showsSwipeRing
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

        readonly property real innerRadius: Math.hypot(root.width, root.height) / 2 + Theme.keyboard.keySpacing
        readonly property real outerRadius: innerRadius + root.popupCellWidth
        readonly property real middleRadius: (innerRadius + outerRadius) / 2

        function sectorAngle(index) {
            return (root.swipeArcStart + index * root.swipeStep) * Math.PI / 180
        }

        visible: root.showsSwipeRing && swipePreview.highlighted >= 0
        anchors.centerIn: parent
        width: 2 * outerRadius
        height: 2 * outerRadius

        onVisibleChanged: sectors.requestPaint()

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
                var halfStep = root.swipeStep * Math.PI / 360
                context.lineWidth = Theme.border.thin
                context.strokeStyle = Theme.keyboard.previewBorder
                for (var index = 0; index < root.swipes.length; ++index) {
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
            model: root.showsSwipeRing ? root.swipes : []

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

        anchors.fill: parent
        preventStealing: true
        pressAndHoldInterval: Theme.keyboard.repeatDelay

        onPressed: {
            consumed = false
            felt = -1
            KeyboardHaptics.keyPress()
            root.touched()
        }

        onPressAndHold: (mouse) => {
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

        onPositionChanged: (mouse) => {
            if (alternatesPopup.visible)
                area.highlightAlternateAt(mouse.x, mouse.y)
            else if (root.swipes.length > 0 && !repeatTimer.running)
                area.trackSwipe(mouse.x, mouse.y)
        }

        onReleased: {
            repeatTimer.stop()
            var swiped = swipePreview.character
            swipePreview.highlighted = -1
            if (alternatesPopup.visible) {
                alternatesPopup.visible = false
                root.activated(root.alternates[alternatesPopup.highlighted])
            } else if (swiped !== "") {
                root.activated(swiped)
            } else if (!consumed && containsMouse) {
                root.activated(root.value)
            }
        }

        onCanceled: {
            repeatTimer.stop()
            alternatesPopup.visible = false
            swipePreview.highlighted = -1
        }
    }
}
