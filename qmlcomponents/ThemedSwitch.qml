import QtQuick
import QtQuick.Controls

import Themed.Components

Switch {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding,
                             implicitIndicatorHeight + topPadding + bottomPadding)

    indicator: Rectangle {
        implicitWidth: 48
        implicitHeight: 26
        x: control.leftPadding
        y: parent.height / 2 - height / 2
        radius: height / 2
        color: control.checked ? Theme.colors.primary : Theme.colors.border
        border.width: 0

        Behavior on color { ColorAnimation { duration: 200 } }

        Rectangle {
            x: control.checked ? parent.width - width - 3 : 3
            y: (parent.height - height) / 2
            width: 20
            height: 20
            radius: height / 2
            color: Theme.colors.textInverse
            border.width: 0

            Behavior on x {
                NumberAnimation { duration: 200; easing.type: Easing.InOutQuad }
            }
        }
    }

    contentItem: Text {
        text: control.text
        font.pixelSize: Theme.fontSize.medium
        color: Theme.colors.textPrimary
        verticalAlignment: Text.AlignVCenter
        leftPadding: control.indicator.width + Theme.spacing.medium
    }
}
