import QtQuick

import Themed.Components

Rectangle {
    id: control

    property bool vertical: false

    implicitWidth: control.vertical ? Theme.separator.thickness : 0
    implicitHeight: control.vertical ? 0 : Theme.separator.thickness
    color: Theme.separator.color
}
