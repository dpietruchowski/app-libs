import QtQuick
import Themed.Components

Rectangle {
    id: root

    property int count: 0
    property int maxCount: 1
    property bool future: false
    property bool today: false

    readonly property bool filled: count > 0
    readonly property real level: maxCount > 0 ? Math.min(1.0, count / maxCount) : 1.0
    readonly property real tint: Theme.activity.minTint
        + (Theme.activity.maxTint - Theme.activity.minTint) * level
    readonly property color tone: future ? Theme.colors.secondary : Theme.colors.primary

    color: filled ? Qt.rgba(tone.r, tone.g, tone.b, tint) : "transparent"
    border.color: today
        ? Theme.colors.textPrimary
        : (filled ? tone : Theme.colors.textSecondary)
    border.width: today ? Theme.border.medium : Theme.border.thin
}
