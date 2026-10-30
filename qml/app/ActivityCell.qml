import QtQuick
import Themed.Components

Rectangle {
    id: root

    property bool future: false
    property bool today: false
    property int count: 0
    property int maxCount: 1

    readonly property var pastAlpha: [0.25, 0.35, 0.45, 0.55, 0.65, 0.75, 0.87, 1.0]
    readonly property var futureAlpha: [0.35, 0.55, 0.8, 1.0]
    readonly property int heat: {
        if (count <= 0)
        {
            return 0
        }
        if (future)
        {
            return Math.min(4, Math.ceil(4 * count / Math.max(1, maxCount)))
        }
        return Math.min(pastAlpha.length, count)
    }

    readonly property bool filled: heat > 0

    function tint(base, alpha) {
        return Qt.rgba(base.r, base.g, base.b, alpha)
    }

    radius: Math.max(Theme.radius.small / 2, width * 0.25)
    color: heat === 0
           ? (future ? "transparent" : Theme.colors.border)
           : tint(future ? Theme.activity.scheduled : Theme.colors.primary,
                  (future ? futureAlpha : pastAlpha)[heat - 1])
    border.width: today || heat === 0 ? Theme.border.thin : 0
    border.color: today ? Theme.colors.textPrimary : Theme.colors.borderStrong
}
