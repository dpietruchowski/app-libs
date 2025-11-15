import QtQuick
import QtQuick.Controls
import FillIn.Components

Item {
    id: root

    implicitWidth: Theme.card.sizeSmall
    implicitHeight: Theme.card.sizeSmall

    property real progress: 0.0
    property color color: Theme.colors.primary
    property real lineWidth: 3
    property real radius: Theme.radius.large

    default property alias content: contentItem.data

    Item {
        id: contentItem
        anchors.fill: parent
    }

    Canvas {
        id: progressCanvas
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()

            if (progress <= 0) return

            var w = width
            var h = height
            var r = radius
            var lw = lineWidth
            var offset = lw / 2
            var innerW = w - lw
            var innerH = h - lw

            var totalPerimeter = 2 * (innerW + innerH) - 8 * r + 2 * Math.PI * r
            var targetLength = totalPerimeter * Math.min(Math.max(progress, 0), 1)

            ctx.strokeStyle = root.color
            ctx.lineWidth = lw
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            ctx.beginPath()

            var currentX = w / 2
            var currentY = offset
            ctx.moveTo(currentX, currentY)

            var remaining = targetLength

            // 1. Top right (from center to right corner)
            var topRightLen = innerW / 2 - r
            if (remaining > 0) {
                var drawn = Math.min(remaining, topRightLen)
                currentX = w / 2 + drawn
                ctx.lineTo(currentX, currentY)
                remaining -= drawn
            }

            // 2. Top-right corner arc
            if (remaining > 0) {
                var arcLen = (Math.PI / 2) * r
                var drawn2 = Math.min(remaining, arcLen)
                var angle = drawn2 / r
                ctx.arc(innerW - r + offset, r + offset, r, -Math.PI / 2, -Math.PI / 2 + angle, false)

                currentX = (innerW - r + offset) + r * Math.cos(-Math.PI / 2 + angle)
                currentY = (r + offset) + r * Math.sin(-Math.PI / 2 + angle)
                remaining -= drawn2
            }

            // 3. Right edge
            if (remaining > 0) {
                var rightEdgeLen = innerH - 2 * r
                var drawn3 = Math.min(remaining, rightEdgeLen)
                currentY = r + offset + drawn3
                ctx.lineTo(innerW + offset, currentY)
                remaining -= drawn3
            }

            // 4. Bottom-right corner arc
            if (remaining > 0) {
                var arcLen2 = (Math.PI / 2) * r
                var drawn4 = Math.min(remaining, arcLen2)
                var angle2 = drawn4 / r
                ctx.arc(innerW - r + offset, innerH - r + offset, r, 0, angle2, false)

                currentX = (innerW - r + offset) + r * Math.cos(angle2)
                currentY = (innerH - r + offset) + r * Math.sin(angle2)
                remaining -= drawn4
            }

            // 5. Bottom edge
            if (remaining > 0) {
                var bottomEdgeLen = innerW - 2 * r
                var drawn5 = Math.min(remaining, bottomEdgeLen)
                currentX = innerW - r + offset - drawn5
                ctx.lineTo(currentX, innerH + offset)
                remaining -= drawn5
            }

            // 6. Bottom-left corner arc
            if (remaining > 0) {
                var arcLen3 = (Math.PI / 2) * r
                var drawn6 = Math.min(remaining, arcLen3)
                var angle3 = drawn6 / r
                ctx.arc(r + offset, innerH - r + offset, r, Math.PI / 2, Math.PI / 2 + angle3, false)

                currentX = (r + offset) + r * Math.cos(Math.PI / 2 + angle3)
                currentY = (innerH - r + offset) + r * Math.sin(Math.PI / 2 + angle3)
                remaining -= drawn6
            }

            // 7. Left edge
            if (remaining > 0) {
                var leftEdgeLen = innerH - 2 * r
                var drawn7 = Math.min(remaining, leftEdgeLen)
                currentY = innerH - r + offset - drawn7
                ctx.lineTo(offset, currentY)
                remaining -= drawn7
            }

            // 8. Top-left corner arc
            if (remaining > 0) {
                var arcLen4 = (Math.PI / 2) * r
                var drawn8 = Math.min(remaining, arcLen4)
                var angle4 = drawn8 / r
                ctx.arc(r + offset, r + offset, r, Math.PI, Math.PI + angle4, false)

                currentX = (r + offset) + r * Math.cos(Math.PI + angle4)
                currentY = (r + offset) + r * Math.sin(Math.PI + angle4)
                remaining -= drawn8
            }

            // 9. Top left (from left corner back to center)
            if (remaining > 0) {
                var topLeftLen = innerW / 2 - r
                var drawn9 = Math.min(remaining, topLeftLen)
                currentX = r + offset + drawn9
                ctx.lineTo(currentX, offset)
                remaining -= drawn9
            }

            ctx.stroke()
        }
    }

    Behavior on progress {
        NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
    }

    onProgressChanged: progressCanvas.requestPaint()
    onWidthChanged: progressCanvas.requestPaint()
    onHeightChanged: progressCanvas.requestPaint()
    onRadiusChanged: progressCanvas.requestPaint()
    onLineWidthChanged: progressCanvas.requestPaint()
    onColorChanged: progressCanvas.requestPaint()

    Component.onCompleted: progressCanvas.requestPaint()
}
