import QtQuick

import Themed.Components

Rectangle {
    id: root

    property KeyboardBuffer buffer
    property string placeholder: ""
    property color backgroundColor: Theme.colors.surface
    property int contentPadding: Theme.padding.medium
    property bool keyNavigable: true

    readonly property string text: buffer ? buffer.text : ""
    readonly property int cursorPosition: buffer ? buffer.cursorPosition : 0

    function keyActivate() {
        root.forceActiveFocus()
    }

    function positionAt(x) {
        var best = 0
        var bestDistance = Math.abs(x)
        for (var position = 1; position <= root.text.length; ++position) {
            probe.text = root.text.substring(0, position)
            var distance = Math.abs(x - probe.advanceWidth)
            if (distance < bestDistance) {
                best = position
                bestDistance = distance
            }
        }
        return best
    }

    function handleKey(event) {
        if (!root.buffer)
            return false
        switch (event.key) {
        case Qt.Key_Backspace:
            root.buffer.backspace()
            return true
        case Qt.Key_Delete:
            root.buffer.deleteForward()
            return true
        case Qt.Key_Left:
            root.buffer.moveCursor(-1)
            return true
        case Qt.Key_Right:
            root.buffer.moveCursor(1)
            return true
        case Qt.Key_Home:
            root.buffer.cursorPosition = 0
            return true
        case Qt.Key_End:
            root.buffer.cursorPosition = root.text.length
            return true
        case Qt.Key_Return:
        case Qt.Key_Enter:
            root.buffer.submit()
            return true
        }
        if (event.modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier))
            return false
        if (event.text === "" || event.text.charCodeAt(0) < 32 || event.text.charCodeAt(0) === 127)
            return false
        root.buffer.insert(event.text)
        return true
    }

    implicitWidth: Theme.applicationWidth * 0.8
    implicitHeight: 52
    activeFocusOnTab: true
    clip: true
    color: root.backgroundColor
    radius: Theme.radius.xLarge
    border.width: root.activeFocus ? Theme.border.medium : Theme.border.thin
    border.color: root.activeFocus ? Theme.colors.primary : Theme.colors.border

    Behavior on border.color { ColorAnimation { duration: 150 } }
    Behavior on border.width { NumberAnimation { duration: 150 } }

    Keys.onPressed: (event) => {
        event.accepted = root.handleKey(event)
    }

    TextMetrics {
        id: beforeCursor
        font: content.font
        text: root.text.substring(0, root.cursorPosition)
    }

    TextMetrics {
        id: probe
        font: content.font
    }

    TextMetrics {
        id: beforeComposition
        font: content.font
        text: root.buffer && root.buffer.composing
              ? root.text.substring(0, root.buffer.compositionStart) : ""
    }

    TextMetrics {
        id: composition
        font: content.font
        text: root.buffer && root.buffer.composing
              ? root.text.substr(root.buffer.compositionStart, root.buffer.compositionLength) : ""
    }

    Item {
        id: viewport

        readonly property real caretX: beforeCursor.advanceWidth
        readonly property real textWidth: content.contentWidth

        anchors.fill: parent
        anchors.leftMargin: root.contentPadding
        anchors.rightMargin: root.contentPadding

        Text {
            objectName: root.objectName === "" ? "" : root.objectName + "Placeholder"
            anchors.centerIn: parent
            visible: root.text === ""
            text: root.placeholder
            font: content.font
            color: Theme.colors.textPlaceholder
        }

        Text {
            id: content
            objectName: root.objectName === "" ? "" : root.objectName + "Text"
            anchors.verticalCenter: parent.verticalCenter
            x: viewport.textWidth + caret.width <= viewport.width
               ? (viewport.width - viewport.textWidth) / 2
               : Math.max(viewport.width - viewport.textWidth - caret.width,
                          Math.min(0, viewport.width - viewport.caretX - caret.width))
            text: root.text
            font.pixelSize: Theme.fontSize.medium
            color: Theme.colors.textPrimary
        }

        Rectangle {
            objectName: root.objectName === "" ? "" : root.objectName + "Composition"
            visible: root.buffer !== null && root.buffer.composing
            x: content.x + beforeComposition.advanceWidth
            y: content.y + content.height
            width: composition.advanceWidth
            height: Theme.border.medium
            color: Theme.keyboard.composingUnderline
        }

        ThemedCaret {
            id: caret
            objectName: root.objectName === "" ? "" : root.objectName + "Caret"
            x: content.x + viewport.caretX
            anchors.verticalCenter: parent.verticalCenter
            height: content.font.pixelSize * 1.3
            visible: root.activeFocus
        }
    }

    TapHandler {
        onTapped: (eventPoint) => {
            root.forceActiveFocus()
            if (root.buffer) {
                var point = viewport.mapFromItem(root, eventPoint.position.x, eventPoint.position.y)
                root.buffer.cursorPosition = root.positionAt(point.x - content.x)
            }
        }
    }
}
