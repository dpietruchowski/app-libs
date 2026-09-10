import QtQuick
import QtQuick.Layouts

import Themed.Components

Rectangle {
    id: control

    property var options: []
    property int currentIndex: 0

    signal activated(int index)

    implicitWidth: row.implicitWidth + 2 * Theme.segmented.padding
    implicitHeight: Theme.segmented.height
    radius: Theme.segmented.radius
    color: Theme.segmented.background
    border.width: Theme.border.thin
    border.color: Theme.segmented.border

    RowLayout {
        id: row

        anchors.fill: parent
        anchors.margins: Theme.segmented.padding
        spacing: Theme.segmented.padding

        Repeater {
            model: control.options

            Rectangle {
                id: segment

                required property var modelData
                required property int index

                readonly property bool selected: index === control.currentIndex

                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.segmented.radius - Theme.segmented.padding
                color: segment.selected ? Theme.segmented.selectedBackground : "transparent"

                Behavior on color { ColorAnimation { duration: 120 } }

                Text {
                    anchors.fill: parent
                    text: String(segment.modelData)
                    color: segment.selected ? Theme.segmented.selectedText : Theme.segmented.text
                    font.pixelSize: Theme.segmented.fontSize
                    font.bold: segment.selected
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        control.currentIndex = segment.index
                        control.activated(segment.index)
                    }
                }
            }
        }
    }
}
