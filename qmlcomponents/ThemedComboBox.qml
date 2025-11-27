import QtQuick
import QtQuick.Controls

import Themed.Components

ComboBox {
    id: control

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
        color: Theme.colors.background
        border.color: Theme.colors.cardBorder
        border.width: Theme.border.thin
        radius: Theme.radius.small
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
        y: control.height
        width: control.width
        implicitHeight: contentItem.implicitHeight
        padding: 1

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex

            ScrollIndicator.vertical: ScrollIndicator { }
        }

        background: Rectangle {
            color: Theme.colors.cardBackground
            border.color: Theme.colors.cardBorder
            border.width: Theme.border.thin
            radius: Theme.radius.small
        }
    }

    delegate: ItemDelegate {
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
