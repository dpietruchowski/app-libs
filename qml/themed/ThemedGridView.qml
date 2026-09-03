import QtQuick
import QtQuick.Controls

import Themed.Components

GridView {
    id: control

    property int columns: Theme.grid.columns

    cellWidth: control.width / Math.max(1, control.columns)
    cellHeight: cellWidth
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    ScrollBar.vertical: ScrollBar {
        policy: control.contentHeight > control.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff

        background: Item {}

        contentItem: Rectangle {
            implicitWidth: Theme.scroll.barWidth
            radius: Theme.scroll.barRadius
            color: Theme.scroll.barColor
            opacity: parent.active ? 1.0 : Theme.scroll.barIdleOpacity
        }
    }
}
