import QtQuick
import QtQuick.Controls

import Themed.Components

Flickable {
    id: control

    contentWidth: width
    contentHeight: contentItem.childrenRect.height
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

            Behavior on opacity { NumberAnimation { duration: 200 } }
        }
    }
}
