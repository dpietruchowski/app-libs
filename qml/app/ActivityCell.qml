import QtQuick
import Themed.Components

Rectangle {
    id: root

    property int count: 0
    property int maxCount: 1
    property bool today: false

    readonly property bool filled: count > 0
    readonly property real level: maxCount > 0 ? Math.min(1.0, count / maxCount) : 1.0
    readonly property real tint: Theme.activity.minTint
        + (Theme.activity.maxTint - Theme.activity.minTint) * level

    color: filled
        ? Qt.rgba(Theme.colors.primary.r, Theme.colors.primary.g, Theme.colors.primary.b, tint)
        : "transparent"
    border.color: today
        ? Theme.colors.textPrimary
        : (filled ? Theme.colors.primary : Theme.colors.textSecondary)
    border.width: today ? Theme.border.medium : Theme.border.thin
}
