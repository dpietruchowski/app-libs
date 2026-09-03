import QtQuick
import QtQuick.Controls

import Themed.Components

Button {
    id: control

    property var buttonSize: Theme.button.medium
    property var buttonStyle: Theme.button.primary
    property url iconSource: ""
    property int iconSize: buttonSize.iconSize
    property color iconColor: buttonStyle.text
    property bool circular: false
    property bool pill: false
    property int radius: Theme.button.radius
    property int contentAlignment: Qt.AlignHCenter
    property int contentPadding: Theme.padding.medium
    property bool keyNavigable: true
    property bool keyDefault: false

    function keyActivate() {
        if (control.checkable)
            control.toggle()
        control.clicked()
    }

    implicitWidth: buttonSize.width
    implicitHeight: buttonSize.height

    background: Rectangle {
        radius: circular ? Math.min(width, height) / 2 : pill ? height / 2 : control.radius
        color: !control.enabled ? Theme.colors.surface :
               control.down ? buttonStyle.pressed :
               control.hovered ? buttonStyle.hovered :
               buttonStyle.background
        border.width: Theme.border.thin
        border.color: buttonStyle.border

        Behavior on color { ColorAnimation { duration: 120 } }
    }

    contentItem: Item {
        id: content

        anchors.fill: parent

        readonly property bool hasIcon: control.iconSource.toString() !== ""
        readonly property bool leftAligned: control.contentAlignment === Qt.AlignLeft
        readonly property real availableTextWidth: width - 2 * control.contentPadding
                                                   - (hasIcon ? control.iconSize + row.spacing : 0)

        Row {
            id: row

            spacing: Theme.spacing.small
            height: parent.height
            anchors.verticalCenter: parent.verticalCenter
            anchors.horizontalCenter: content.leftAligned ? undefined : parent.horizontalCenter
            anchors.left: content.leftAligned ? parent.left : undefined
            anchors.leftMargin: content.leftAligned ? control.contentPadding : 0

            ThemedIcon {
                visible: content.hasIcon
                svgSource: iconSource
                color: !control.enabled ? Theme.colors.textDisabled : control.iconColor
                width: control.iconSize
                height: control.iconSize
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                visible: control.text !== ""
                text: control.text
                width: content.leftAligned ? Math.min(implicitWidth, content.availableTextWidth) : implicitWidth
                elide: Text.ElideRight
                font.pixelSize: buttonSize.fontSize
                color: !control.enabled ? Theme.colors.textDisabled : buttonStyle.text
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: content.leftAligned ? Text.AlignLeft : Text.AlignHCenter
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }
}
