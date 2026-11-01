import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Themed.Components

Flickable {
    id: control

    default property alias content: body.data

    contentWidth: width
    contentHeight: body.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds

    ColumnLayout {
        id: body

        width: control.width
        spacing: Theme.spacing.medium
    }

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
