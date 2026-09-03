import QtQuick

import Themed.Components

Rectangle {
    id: control

    property bool vertical: false

    implicitWidth: control.vertical ? Theme.separator.thickness
                                    : (parent ? parent.width : Theme.applicationWidth)
    implicitHeight: control.vertical ? (parent ? parent.height : Theme.applicationHeight)
                                     : Theme.separator.thickness
    color: Theme.separator.color
}
