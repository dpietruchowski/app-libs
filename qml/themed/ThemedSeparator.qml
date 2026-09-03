import QtQuick

import Themed.Components

Rectangle {
    implicitWidth: parent ? parent.width : Theme.applicationWidth
    implicitHeight: Theme.separator.thickness
    color: Theme.separator.color
}
