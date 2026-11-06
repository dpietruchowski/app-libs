import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Themed.Components

Control {
    id: control

    property string value: ""
    property string placeholder: ""
    property bool keyNavigable: true
    property bool hasValue: control.value !== ""
    property string iconSource: ""
    property int iconSize: 0
    property real popupContentWidth: 0

    default property alias content: column.data

    signal picked(string value)
    signal opening

    function open() {
        popup.open()
    }

    function close() {
        popup.close()
    }

    function keyActivate() {
        control.open()
    }

    leftPadding: Theme.padding.medium
    rightPadding: Theme.padding.medium

    background: Rectangle {
        color: Theme.colors.surface
        border.color: Theme.colors.cardBorder
        border.width: Theme.border.thin
        radius: Theme.radius.xLarge
    }

    contentItem: RowLayout {
        spacing: Theme.spacing.small

        Text {
            Layout.fillWidth: true
            text: control.hasValue ? control.value : control.placeholder
            color: control.hasValue ? Theme.colors.textPrimary : Theme.colors.textPlaceholder
            font.pixelSize: Theme.fontSize.medium
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        ThemedIcon {
            Layout.preferredWidth: control.iconSize
            Layout.preferredHeight: control.iconSize
            svgSource: control.iconSource
            color: Theme.colors.textSecondary
        }
    }

    TapHandler {
        onTapped: control.keyActivate()
    }

    Popup {
        id: popup

        readonly property Item overlayItem: control.Overlay.overlay
        property bool above: false

        objectName: control.objectName ? control.objectName + "_popup" : ""
        y: popup.above ? -popup.implicitHeight - Theme.spacing.xSmall : control.height + Theme.spacing.xSmall
        width: control.popupContentWidth + leftPadding + rightPadding
        margins: Theme.spacing.medium
        padding: Theme.padding.small
        focus: true

        onAboutToShow: {
            control.opening()
            const bottom = control.mapToItem(popup.overlayItem, 0, control.height).y
                           + Theme.spacing.xSmall + popup.implicitHeight + popup.margins
            popup.above = bottom > popup.overlayItem.height
        }

        background: Rectangle {
            color: Theme.colors.surface
            border.color: Theme.colors.cardBorder
            border.width: Theme.border.thin
            radius: Theme.radius.xLarge
        }

        contentItem: ColumnLayout {
            id: column

            spacing: Theme.spacing.small
        }
    }
}
