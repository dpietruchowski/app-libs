import QtQuick
import QtQuick.Controls

import Themed.Components

ComboBox {
    id: control

    property bool keyNavigable: true

    function keyActivate() {
        control.forceActiveFocus()
        control.popup.open()
    }

    font.pixelSize: Theme.fontSize.medium
    implicitHeight: 44
    implicitWidth: Math.max(background.implicitWidth, contentItem.implicitWidth + leftPadding + rightPadding + indicator.width)

    Component.onCompleted: {
        var maxWidth = 0
        for (var i = 0; i < model.length; i++) {
            var itemText = model[i]
            maxWidth = Math.max(maxWidth, textMetrics.advanceWidth)
            textMetrics.text = itemText
        }
        implicitWidth = maxWidth + Theme.padding.medium * 2 + indicator.width + Theme.spacing.medium
    }

    TextMetrics {
        id: textMetrics
        font: control.font
    }

    background: Rectangle {
        color: Theme.colors.surface
        border.color: Theme.colors.cardBorder
        border.width: Theme.border.thin
        radius: Theme.radius.xLarge
    }

    contentItem: Text {
        text: control.displayText
        color: Theme.colors.textPrimary
        font: control.font
        verticalAlignment: Text.AlignVCenter
        leftPadding: Theme.padding.medium
        rightPadding: Theme.padding.medium
        elide: Text.ElideRight
    }

    popup: Popup {
        id: popup
        y: control.height
        width: control.width
        padding: 1

        readonly property Item overlayItem: control.Overlay.overlay
        topMargin: (overlayItem?.SafeArea?.margins.top ?? 0) + Theme.spacing.medium
        bottomMargin: (overlayItem?.SafeArea?.margins.bottom ?? 0) + Theme.spacing.medium

        readonly property real maxPopupHeight: {
            if (!overlayItem)
                return Number.MAX_VALUE
            var topInOverlay = control.mapToItem(overlayItem, 0, control.height).y
            return Math.max(Theme.navigation.barHeight,
                            overlayItem.height - topInOverlay - bottomMargin)
        }

        height: Math.min(contentItem.implicitHeight + topPadding + bottomPadding, maxPopupHeight)

        enter: Transition {
            NumberAnimation {
                property: "opacity"
                from: 0.0
                to: 1.0
                duration: 140
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                property: "scale"
                from: 0.92
                to: 1.0
                duration: 140
                easing.type: Easing.OutCubic
            }
        }

        exit: Transition {
            NumberAnimation {
                property: "opacity"
                from: 1.0
                to: 0.0
                duration: 100
                easing.type: Easing.InCubic
            }
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex

            ScrollIndicator.vertical: ScrollIndicator { }
        }

        background: Rectangle {
            color: Theme.colors.surface
            border.color: Theme.colors.cardBorder
            border.width: Theme.border.thin
            radius: Theme.radius.xLarge
        }
    }

    delegate: ItemDelegate {
        objectName: control.objectName ? control.objectName + "_item" + index : ""
        width: control.width
        contentItem: Text {
            text: control.textRole ? (Array.isArray(control.model) ? modelData[control.textRole] : model[control.textRole]) : modelData
            color: Theme.colors.textPrimary
            font: control.font
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            leftPadding: Theme.padding.medium
        }
        highlighted: control.highlightedIndex === index
        background: Rectangle {
            color: highlighted ? Theme.colors.primary : "transparent"
            opacity: highlighted ? 0.1 : 1.0
        }
    }
}
