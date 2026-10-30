import QtQuick
import Themed.Components

Item {
    id: root

    property string content: ""
    property list<Item> actions
    property int actionOffset: Theme.spacing.medium
    property alias styled: label.styled
    property alias maxLines: label.maxLines
    property alias slotPosition: label.slotPosition
    property alias slotLength: label.slotLength
    property alias font: label.font
    property alias textObjectName: label.objectName
    readonly property alias textItem: label

    implicitWidth: label.implicitWidth
    implicitHeight: internal.actionsTop + Math.max(label.height, internal.actionsBottom)
    height: implicitHeight

    FontMetrics {
        id: metrics
        font: label.font
    }

    QtObject {
        id: internal

        property bool breakBeforeLastWord: false
        property real actionsBottom: 0
        property real actionsTop: 0

        readonly property var visibleActions: {
            const result = []
            for (let i = 0; i < root.actions.length; i++) {
                if (root.actions[i].visible)
                    result.push(root.actions[i])
            }
            return result
        }

        readonly property string suffix: {
            let result = ""
            for (const action of visibleActions)
                result += "&nbsp;" + placeholder(action.width, Math.floor(metrics.ascent))
            return result
        }

        readonly property string laidOutContent: {
            if (!breakBeforeLastWord)
                return root.content
            const space = lastSpaceOutsideTags(root.content)
            return space < 0 ? root.content
                             : root.content.slice(0, space) + "<br>" + root.content.slice(space + 1)
        }

        function placeholder(width, height) {
            const svg = '<svg xmlns="http://www.w3.org/2000/svg" width="' + width
                    + '" height="' + height + '"/>'
            return '<img src="data:image/svg+xml;utf8,' + encodeURIComponent(svg)
                    + '" width="' + width + '" height="' + height + '">'
        }

        function lastSpaceOutsideTags(markup) {
            let insideTag = false
            for (let i = markup.length - 1; i >= 0; i--) {
                const c = markup[i]
                if (c === ">")
                    insideTag = true
                else if (c === "<")
                    insideTag = false
                else if (c === " " && !insideTag)
                    return i
            }
            return -1
        }

        function lastWordStart(plain) {
            for (let i = plain.length - 1; i >= 0; i--) {
                if (/\s/.test(plain[i]))
                    return i + 1
            }
            return 0
        }

        function refresh() {
            const actions = visibleActions
            if (actions.length === 0) {
                actionsBottom = 0
                actionsTop = 0
                return
            }

            const suffixStart = label.length - 2 * actions.length
            let top = 0
            let bottom = 0
            for (let i = 0; i < actions.length; i++) {
                const rect = label.rectAt(suffixStart + 2 * i + 1)
                const offset = rect.y > 0 ? root.actionOffset : 0
                actions[i].x = rect.x
                actions[i].y = Math.round(rect.y + (rect.height - actions[i].height) / 2 + offset)
                top = Math.min(top, actions[i].y)
                bottom = Math.max(bottom, actions[i].y + actions[i].height)
            }
            actionsTop = -top
            actionsBottom = bottom
        }

        function measure() {
            const count = visibleActions.length
            if (count === 0) {
                breakBeforeLastWord = false
                return
            }
            const suffixStart = measurement.length - 2 * count
            const wordY = measurement.rectAt(lastWordStart(measurement.getText(0, Math.max(0, suffixStart)))).y
            breakBeforeLastWord = measurement.rectAt(suffixStart + 1).y !== wordY
                    || measurement.rectAt(measurement.length - 1).y !== wordY
        }

        function adopt() {
            for (let i = 0; i < root.actions.length; i++)
                root.actions[i].parent = label
            schedule()
        }

        function schedule() {
            Qt.callLater(internal.refresh)
        }
    }

    onActionsChanged: internal.adopt()
    Component.onCompleted: internal.adopt()

    ThemedHtmlText {
        id: measurement

        visible: false
        width: root.width
        styled: label.styled
        font: label.font
        content: root.content + internal.suffix

        onTextChanged: internal.measure()
        onWidthChanged: internal.measure()
        onContentWidthChanged: internal.measure()
        onContentHeightChanged: internal.measure()
    }

    ThemedHtmlText {
        id: label

        y: internal.actionsTop
        width: root.width
        content: internal.laidOutContent + internal.suffix

        onTextChanged: internal.schedule()
        onWidthChanged: internal.schedule()
        onContentWidthChanged: internal.schedule()
        onContentHeightChanged: internal.schedule()
    }
}
