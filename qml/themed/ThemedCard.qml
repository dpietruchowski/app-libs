import QtQuick
import QtQuick.Controls

import Themed.Components

Control {
    id: control

    default property alias content: contentArea.contentItem
    property bool clickable: false
    property int margins: 0
    property color backgroundColor: Theme.colors.cardBackground
    property color borderColor: Theme.colors.cardBorder
    property real maxImplicitWidth: -1
    property bool keyNavigable: clickable
    signal clicked()

    function keyActivate() {
        control.clicked()
    }

    implicitWidth: {
        var baseWidth = Math.max(contentArea.implicitWidth, Theme.card.sizeSmall);
        if (maxImplicitWidth > 0) {
            return Math.min(baseWidth, maxImplicitWidth);
        }
        return baseWidth;
    }
    implicitHeight: Math.max(contentArea.implicitHeight, Theme.card.sizeSmall)

    background: Item {
        Repeater {
            model: Theme.elevation.shadowSteps
            Rectangle {
                required property int index
                property real grow: Theme.elevation.shadowBlur * ((index + 1) / Theme.elevation.shadowSteps)
                x: face.x - grow
                y: face.y - grow + Theme.elevation.shadowOffset
                width: face.width + 2 * grow
                height: face.height + 2 * grow
                radius: face.radius + grow
                color: Theme.elevation.shadowColor
                opacity: Theme.elevation.shadowLayerOpacity * (1 - index / Theme.elevation.shadowSteps)
            }
        }

        Rectangle {
            id: face
            anchors.fill: parent
            color: backgroundColor
            radius: Theme.radius.xLarge
            border.color: control.borderColor
            border.width: Theme.border.thin

            Behavior on color { ColorAnimation { duration: 120 } }

            states: [
                State {
                    name: "hovered"
                    when: clickable && mouseArea.containsMouse
                    PropertyChanges {
                        target: face
                        color: Theme.isNightMode ? Qt.lighter(backgroundColor, 1.3) : Qt.darker(backgroundColor, 1.1)
                    }
                },
                State {
                    name: "pressed"
                    when: clickable && mouseArea.pressed
                    PropertyChanges {
                        target: face
                        color: Theme.isNightMode ? Qt.lighter(backgroundColor, 1.15) : Qt.darker(backgroundColor, 1.2)
                    }
                }
            ]
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        enabled: clickable
        hoverEnabled: clickable
        cursorShape: clickable ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: control.clicked()
    }

    Control {
        id: contentArea
        anchors.fill: parent
        anchors.margins: control.margins
    }
}
