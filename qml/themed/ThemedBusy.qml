import QtQuick

import Themed.Components

Item {
    id: control

    property bool running: true
    property int size: Theme.busy.size
    property int dotSize: Theme.busy.dotSize

    implicitWidth: control.size
    implicitHeight: control.size
    visible: control.running

    Item {
        id: spinner

        anchors.fill: parent

        Repeater {
            model: Theme.busy.dotCount

            Rectangle {
                required property int index

                width: control.dotSize
                height: control.dotSize
                radius: width / 2
                color: Theme.busy.color
                opacity: 1 - index / Theme.busy.dotCount
                x: spinner.width / 2 - width / 2
                   + Math.cos(index * 2 * Math.PI / Theme.busy.dotCount)
                     * (spinner.width / 2 - width / 2)
                y: spinner.height / 2 - height / 2
                   + Math.sin(index * 2 * Math.PI / Theme.busy.dotCount)
                     * (spinner.height / 2 - height / 2)
            }
        }

        RotationAnimator on rotation {
            running: control.running
            from: 0
            to: 360
            duration: Theme.busy.duration
            loops: Animation.Infinite
        }
    }
}
