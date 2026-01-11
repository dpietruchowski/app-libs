import QtQuick
import QtQuick.Controls

import Themed.Components

Control {
    id: control

    default property alias content: contentArea.contentItem
    property bool clickable: false
    property int margins: 0
    property color backgroundColor: Theme.colors.cardBackground
    property real maxImplicitWidth: -1
    signal clicked()

    implicitWidth: {
        var baseWidth = Math.max(contentArea.implicitWidth, Theme.card.sizeSmall);
        if (maxImplicitWidth > 0) {
            return Math.min(baseWidth, maxImplicitWidth);
        }
        return baseWidth;
    }
    implicitHeight: Math.max(contentArea.implicitHeight, Theme.card.sizeSmall)

    background: Rectangle {
        color: backgroundColor
        radius: Theme.radius.large
        border.color: Theme.colors.cardBorder
        border.width: Theme.border.thin

        Behavior on color { ColorAnimation { duration: 120 } }

        states: [
            State {
                name: "hovered"
                when: clickable && mouseArea.containsMouse
                PropertyChanges {
                    target: control.background
                    color: Qt.lighter(backgroundColor,5)
                }
            },
            State {
                name: "pressed"
                when: clickable && mouseArea.pressed
                PropertyChanges {
                    target: control.background
                    color: Qt.darker(backgroundColor,5)
                }
            }
        ]
    }

    Control {
        id: contentArea
        anchors.fill: parent
        anchors.margins: control.margins
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        enabled: clickable
        hoverEnabled: clickable
        cursorShape: clickable ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: control.clicked()
    }
}
