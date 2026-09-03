import QtQuick
import QtQuick.Controls

import Themed.Components

Slider {
    id: control

    property bool keyNavigable: true

    function keyActivate() {
        control.forceActiveFocus()
    }

    implicitWidth: Theme.slider.width
    implicitHeight: Theme.slider.handleSize

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth
        height: Theme.slider.trackHeight
        radius: height / 2
        color: Theme.slider.trackColor

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: parent.radius
            color: Theme.slider.fillColor
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        implicitWidth: Theme.slider.handleSize
        implicitHeight: Theme.slider.handleSize
        radius: width / 2
        color: control.pressed ? Theme.slider.handlePressedColor : Theme.slider.handleColor
        border.width: Theme.border.medium
        border.color: Theme.slider.fillColor

        Behavior on color { ColorAnimation { duration: 120 } }
    }
}
