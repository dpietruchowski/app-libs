import QtQuick
import Themed.Components

Item {
    id: root

    property string content: ""
    property list<Item> actions
    property int actionOffset: Theme.spacing.medium
    property int maxLines: 0
    property int slotPosition: -1
    property int slotLength: 0
    property alias styled: label.styled
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
        property string fittedContent: root.content
        property int keptLength: -1

        readonly property var voidTags: ["br", "img", "hr"]
        readonly property bool slotKept: keptLength < 0 || root.slotPosition + root.slotLength <= keptLength

        onSuffixChanged: scheduleFit()

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
                return fittedContent
            const space = lastSpaceOutsideTags(fittedContent)
            return space < 0 ? fittedContent
                             : fittedContent.slice(0, space) + "<br>" + fittedContent.slice(space + 1)
        }

        function placeholder(width, height) {
            const svg = '<svg xmlns="http://www.w3.org/2000/svg" width="' + width
                    + '" height="' + height + '"/>'
            return '<img src="data:image/svg+xml;utf8,' + encodeURIComponent(svg)
                    + '" width="' + width + '" height="' + height + '">'
        }

        function spacesOutsideTags(markup) {
            const spaces = []
            let insideTag = false
            for (let i = 0; i < markup.length; i++) {
                const c = markup[i]
                if (c === "<")
                    insideTag = true
                else if (c === ">")
                    insideTag = false
                else if (c === " " && !insideTag)
                    spaces.push(i)
            }
            return spaces
        }

        function lastSpaceOutsideTags(markup) {
            const spaces = spacesOutsideTags(markup)
            return spaces.length > 0 ? spaces[spaces.length - 1] : -1
        }

        function closingTags(markup) {
            const open = []
            const tag = /<(\/?)([a-zA-Z][a-zA-Z0-9]*)[^>]*?(\/?)>/g
            let match = tag.exec(markup)
            while (match !== null) {
                if (match[1] === "/")
                    open.pop()
                else if (match[3] !== "/" && !voidTags.includes(match[2].toLowerCase()))
                    open.push(match[2])
                match = tag.exec(markup)
            }
            return open.reverse().map(name => "</" + name + ">").join("")
        }

        function keptHead(spaceIndex) {
            const head = root.content.slice(0, spaceIndex)
            return head + closingTags(head)
        }

        function lineCountOf(markup) {
            fitter.content = markup + suffix
            return Math.round(fitter.implicitHeight / metrics.lineSpacing)
        }

        function fits(markup) {
            return lineCountOf(markup) <= root.maxLines
        }

        function fit() {
            const spaces = spacesOutsideTags(root.content)
            if (root.maxLines <= 0 || root.width <= 0 || spaces.length === 0 || fits(root.content)) {
                fittedContent = root.content
                keptLength = -1
                return
            }

            let low = 0
            let high = spaces.length - 1
            while (low < high) {
                const middle = Math.ceil((low + high) / 2)
                if (fits(keptHead(spaces[middle]) + "…"))
                    low = middle
                else
                    high = middle - 1
            }

            const kept = keptHead(spaces[low])
            fittedContent = kept + "…"
            fitter.content = kept
            keptLength = fitter.length
        }

        function scheduleFit() {
            Qt.callLater(internal.fit)
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
    onContentChanged: internal.scheduleFit()
    onWidthChanged: internal.scheduleFit()
    onMaxLinesChanged: internal.scheduleFit()
    onFontChanged: internal.scheduleFit()
    Component.onCompleted: {
        internal.adopt()
        internal.scheduleFit()
    }

    ThemedHtmlText {
        id: fitter

        visible: false
        width: root.width
        styled: label.styled
        font: label.font
    }

    ThemedHtmlText {
        id: measurement

        visible: false
        width: root.width
        styled: label.styled
        font: label.font
        content: internal.fittedContent + internal.suffix

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
        slotPosition: internal.slotKept ? root.slotPosition : -1
        slotLength: root.slotLength

        onTextChanged: internal.schedule()
        onWidthChanged: internal.schedule()
        onContentWidthChanged: internal.schedule()
        onContentHeightChanged: internal.schedule()
    }
}
