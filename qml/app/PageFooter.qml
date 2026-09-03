import QtQuick
import Themed.Components

Item {
    id: root

    default property alias content: row.data

    implicitHeight: row.implicitHeight + Theme.spacing.small * 2

    Row {
        id: row
        anchors.centerIn: parent
        spacing: Theme.spacing.medium
    }
}
