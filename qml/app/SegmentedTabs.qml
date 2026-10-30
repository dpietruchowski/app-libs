import QtQuick
import QtQml.Models
import Themed.Components

Rectangle {
    id: root

    property var tabs: []
    property int currentIndex: 0
    property real maximumWidth: -1
    property string tabObjectNamePrefix: "segmentedTab"

    readonly property real inset: Theme.scaled(3)
    readonly property real labelPadding: Theme.spacing.small
    readonly property var naturalWidths: {
        const widths = []
        for (let i = 0; i < labelMetrics.count; ++i)
        {
            widths.push(Math.max(Theme.scaled(72),
                                 labelMetrics.objectAt(i).advanceWidth + Theme.padding.medium * 2))
        }
        return widths
    }
    readonly property real naturalTotal: naturalWidths.reduce((sum, value) => sum + value, 0)
    readonly property real widthScale: maximumWidth > 0 && naturalTotal > 0
                                       ? Math.min(1, (maximumWidth - inset * 2) / naturalTotal)
                                       : 1

    implicitWidth: naturalTotal * widthScale + inset * 2
    implicitHeight: Theme.button.icon.height
    radius: Theme.button.radius
    color: "transparent"
    border.width: Theme.border.thin
    border.color: Theme.button.ghost.border

    Instantiator {
        id: labelMetrics
        model: root.tabs

        TextMetrics {
            required property string modelData
            text: modelData
            font.pixelSize: Theme.fontSize.normal
            font.bold: true
        }
    }

    Row {
        x: root.inset
        y: root.inset
        height: root.height - root.inset * 2

        Repeater {
            model: root.tabs

            Rectangle {
                id: tab

                required property int index
                required property string modelData
                readonly property bool selected: index === root.currentIndex

                objectName: root.tabObjectNamePrefix + index
                width: (root.naturalWidths[index] ?? 0) * root.widthScale
                height: parent.height
                radius: root.radius - root.inset
                color: tab.selected
                       ? Theme.colors.surface
                       : (tabMouse.containsMouse ? Theme.button.ghost.hovered : "transparent")

                Behavior on color {
                    ColorAnimation { duration: 120 }
                }

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: root.labelPadding
                    anchors.rightMargin: root.labelPadding
                    text: tab.modelData
                    font.pixelSize: Theme.fontSize.normal
                    font.bold: tab.selected
                    fontSizeMode: Text.HorizontalFit
                    minimumPixelSize: Theme.fontSize.xSmall
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: tab.selected ? Theme.colors.textPrimary : Theme.colors.textSecondary
                }

                MouseArea {
                    id: tabMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.currentIndex = tab.index
                }
            }
        }
    }
}
