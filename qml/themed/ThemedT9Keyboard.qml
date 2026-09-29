import QtQuick

import Themed.Components

Rectangle {
    id: root

    property KeyboardBuffer buffer
    property string language: ""
    readonly property var languageLayout: layouts.layoutFor(language)
    property var letterGroups: languageLayout.t9
    property var symbolGroups: layouts.t9Symbols
    property string punctuation: layouts.t9Punctuation
    property string enterLabel: ""
    property bool symbolsActive: false
    property var predictor: null
    property bool predictiveEnabled: true
    property int candidateLimit: 8
    property string keyLayout: "phone"
    property var buckets: layouts.t9QwertyTriangles

    readonly property var groups: symbolsActive ? symbolGroups : letterGroups
    readonly property var digitKeys: [root.punctuation].concat(root.groups).map((group, index) => ({
        name: String(index + 1),
        code: String(index + 1),
        letters: group,
        value: group + String(index + 1),
        characters: Array.from(group + String(index + 1)),
        predictive: index > 0
    }))
    readonly property var bucketKeys: root.buckets
        .map((letters, index) => root.bucketKey(letters, (index + 1).toString(36)))
        .filter(key => key.cells.length > 0)
    readonly property var punctuationKey: ({
        name: "punctuation",
        code: "",
        letters: root.punctuation.substring(0, 3),
        value: root.punctuation,
        characters: Array.from(root.punctuation),
        predictive: false
    })
    readonly property bool qwertyShape: keyLayout === "qwerty" && !symbolsActive
    readonly property var keyMap: (qwertyShape ? bucketKeys : digitKeys)
        .filter(key => key.predictive)
        .reduce((map, key) => Object.assign(map, { [key.code]: key.letters }), {})
    readonly property real innerWidth: width - 2 * Theme.keyboard.padding
    readonly property real unit: (innerWidth - 3 * Theme.keyboard.keySpacing)
                                 / (3 + Theme.keyboard.t9SideKeyUnits)
    readonly property real qwertyUnit: (innerWidth - 9 * Theme.keyboard.keySpacing) / 10
    readonly property real qwertySpecialWidth: qwertyUnit * Theme.keyboard.specialKeyUnits
    readonly property real qwertyKeyHeight: Theme.keyboard.keyHeight
    readonly property real sideWidth: unit * Theme.keyboard.t9SideKeyUnits
    readonly property real keyHeight: Theme.keyboard.t9KeyHeight
    readonly property real tallKeyHeight: 2 * keyHeight + Theme.keyboard.rowSpacing
    readonly property bool upperCase: buffer !== null && buffer.upperCase && !symbolsActive
    readonly property bool predictionAvailable: predictor !== null
                                                && ["", "en"].includes(layouts.codeFor(language))
    readonly property bool predicting: predictionAvailable && predictiveEnabled && !symbolsActive
    readonly property real candidateBarSpace: predictionAvailable
                                              ? Theme.keyboard.candidateBarHeight
                                                + Theme.keyboard.rowSpacing
                                              : 0

    signal keyTapped()

    function compose(characters) {
        if (!root.buffer)
            return
        root.buffer.compose(characters)
        commitTimer.restart()
    }

    function typeKey(key) {
        if (root.buffer)
            root.buffer.typeKey(key)
    }

    function tapShift() {
        if (!root.buffer)
            return
        var now = Date.now()
        if (now - shiftTaps.last < Theme.keyboard.doubleTapInterval) {
            root.buffer.lockShift()
            shiftTaps.last = 0
        } else {
            root.buffer.toggleShift()
            shiftTaps.last = now
        }
    }

    function letterCell(letter) {
        var rows = layouts.qwerty
        var row = rows.findIndex(letters => letters.includes(letter))
        if (row < 0)
            return null
        var step = root.qwertyUnit + Theme.keyboard.keySpacing
        return {
            letter: letter,
            row: row,
            x: (root.innerWidth - rows[row].length * step + Theme.keyboard.keySpacing) / 2
               + rows[row].indexOf(letter) * step,
            y: row * (root.qwertyKeyHeight + Theme.keyboard.rowSpacing),
            width: root.qwertyUnit,
            height: root.qwertyKeyHeight
        }
    }

    function rowSpans(cells) {
        return cells.map(cell => cell.row)
            .filter((row, index, rows) => rows.indexOf(row) === index)
            .sort()
            .map(row => {
                var inRow = cells.filter(cell => cell.row === row)
                return {
                    row: row,
                    y: inRow[0].y,
                    left: Math.min(...inRow.map(cell => cell.x)),
                    right: Math.max(...inRow.map(cell => cell.x + cell.width))
                }
            })
    }

    function withoutStraightCorners(points) {
        return points.filter((point, index) => {
            var previous = points[(index + points.length - 1) % points.length]
            var next = points[(index + 1) % points.length]
            var turn = (point.x - previous.x) * (next.y - point.y)
                       - (point.y - previous.y) * (next.x - point.x)
            return Math.abs(turn) > 0.01
        })
    }

    function bucketOutline(cells) {
        var spans = root.rowSpans(cells)
        var height = root.qwertyKeyHeight
        var rightSide = [].concat(...spans.map(span => [
            { x: span.right, y: span.y },
            { x: span.right, y: span.y + height }
        ]))
        var leftSide = [].concat(...spans.slice().reverse().map(span => [
            { x: span.left, y: span.y + height },
            { x: span.left, y: span.y }
        ]))
        return root.withoutStraightCorners(rightSide.concat(leftSide))
    }

    function polygonContains(points, point) {
        return points.reduce((inside, corner, index) => {
            var previous = points[(index + points.length - 1) % points.length]
            var crosses = (corner.y > point.y) !== (previous.y > point.y)
                          && point.x < (previous.x - corner.x) * (point.y - corner.y)
                                       / (previous.y - corner.y) + corner.x
            return crosses ? !inside : inside
        }, false)
    }

    function bucketKey(letters, code) {
        var cells = Array.from(letters).map(letter => root.letterCell(letter))
            .filter(cell => cell !== null)
        var outline = root.bucketOutline(cells)
        var left = Math.min(...outline.map(point => point.x))
        var top = Math.min(...outline.map(point => point.y))
        var relative = (item) => Object.assign({}, item, { x: item.x - left, y: item.y - top })
        return {
            name: letters,
            code: code,
            letters: letters,
            value: letters,
            characters: Array.from(letters),
            predictive: true,
            x: left,
            y: top,
            width: Math.max(...outline.map(point => point.x)) - left,
            height: Math.max(...outline.map(point => point.y)) - top,
            cells: cells.map(relative),
            outline: outline.map(relative)
        }
    }

    function showWord(text) {
        word.updating = true
        root.buffer.composeWord(text)
        word.updating = false
        word.shown = text
    }

    function lookUpCandidates() {
        var groups = Array.from(word.codes).map(code => root.keyMap[code])
        word.candidates = root.predictor.candidatesForGroups(groups, root.candidateLimit)
    }

    function typeCode(code) {
        if (!root.buffer || !(code in root.keyMap))
            return
        var previous = word.shown
        word.codes += code
        root.lookUpCandidates()
        root.showWord(word.candidates.length > 0
                      ? word.candidates[0].substring(0, word.codes.length)
                      : previous + root.keyMap[code].charAt(0))
    }

    function eraseCode() {
        word.codes = word.codes.slice(0, -1)
        if (word.codes === "") {
            root.showWord("")
            root.resetWord()
            return
        }
        var previous = word.shown
        root.lookUpCandidates()
        root.showWord(word.candidates.length > 0
                      ? word.candidates[0].substring(0, word.codes.length)
                      : previous.slice(0, -1))
    }

    function chooseCandidate(index) {
        root.showWord(word.candidates[index])
        root.acceptWord()
    }

    function acceptWord() {
        if (word.codes === "")
            return
        var accepted = word.shown
        var known = word.candidates.indexOf(accepted) >= 0
        root.resetWord()
        root.buffer.commitComposition()
        if (known) {
            root.predictor.learn(accepted)
            word.accepted = accepted
        }
    }

    function learnTypedWord() {
        if (!root.predictionAvailable || !root.buffer)
            return
        var typed = root.buffer.text.substring(0, root.buffer.cursorPosition).match(/[A-Za-z]+$/)
        if (typed && typed[0] !== word.accepted)
            root.predictor.learn(typed[0])
    }

    function finishWord() {
        if (word.codes !== "")
            root.acceptWord()
        else
            root.learnTypedWord()
        word.accepted = ""
    }

    function resetWord() {
        word.codes = ""
        word.candidates = []
        word.shown = ""
    }

    function activateGroupKey(key, value) {
        if (value === key.value && root.predicting && key.predictive) {
            root.typeCode(key.code)
            return
        }
        root.acceptWord()
        if (value === key.value)
            root.compose(key.characters)
        else
            root.typeKey(value)
    }

    function pressShift() {
        root.acceptWord()
        root.tapShift()
    }

    function typeSpace(value) {
        if (!root.buffer)
            return
        root.finishWord()
        root.buffer.insert(value)
    }

    function pressBackspace() {
        if (!root.buffer)
            return
        if (word.codes !== "")
            root.eraseCode()
        else
            root.buffer.backspace()
    }

    function pressEnter() {
        if (!root.buffer)
            return
        root.finishWord()
        root.buffer.submit()
    }

    function toggleSymbols() {
        root.acceptWord()
        if (root.buffer)
            root.buffer.commitComposition()
        root.symbolsActive = !root.symbolsActive
    }

    objectName: "t9Keyboard"
    implicitWidth: Theme.applicationWidth
    implicitHeight: candidateBarSpace + 4 * (qwertyShape ? qwertyKeyHeight : keyHeight)
                    + 3 * Theme.keyboard.rowSpacing
                    + 2 * Theme.keyboard.padding
    color: Theme.keyboard.background

    onPredictingChanged: {
        if (!root.predicting)
            root.acceptWord()
    }
    onKeyLayoutChanged: root.acceptWord()

    ThemedKeyboardLayouts {
        id: layouts
    }

    component BucketKey: Item {
        id: bucketKey

        required property var modelData
        readonly property bool pressed: bucketArea.pressed

        objectName: "t9Bucket_" + modelData.name
        x: modelData.x
        y: modelData.y
        width: modelData.width
        height: modelData.height
        z: pressed ? 1 : 0
        onPressedChanged: outlineCanvas.requestPaint()
        onModelDataChanged: outlineCanvas.requestPaint()

        Canvas {
            id: outlineCanvas
            anchors.fill: parent
            onPaint: {
                var context = getContext("2d")
                context.reset()
                var points = bucketKey.modelData.outline
                var count = points.length
                var last = points[count - 1]
                context.beginPath()
                context.moveTo((last.x + points[0].x) / 2, (last.y + points[0].y) / 2)
                points.forEach((point, index) => {
                    var previous = points[(index + count - 1) % count]
                    var next = points[(index + 1) % count]
                    var radius = Math.min(Theme.keyboard.keyRadius,
                                          Math.hypot(point.x - previous.x, point.y - previous.y) / 2,
                                          Math.hypot(next.x - point.x, next.y - point.y) / 2)
                    context.arcTo(point.x, point.y, next.x, next.y, radius)
                })
                context.closePath()
                context.fillStyle = bucketKey.pressed ? Theme.keyboard.keyPressedBackground
                                                      : Theme.keyboard.keyBackground
                context.fill()
            }
        }

        Repeater {
            model: bucketKey.modelData.cells

            Text {
                required property var modelData

                objectName: "t9Letter_" + modelData.letter
                x: modelData.x
                y: modelData.y
                width: modelData.width
                height: modelData.height
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: root.upperCase ? modelData.letter.toUpperCase() : modelData.letter
                font.pixelSize: Theme.keyboard.fontSize
                color: Theme.keyboard.keyText
            }
        }

        MouseArea {
            id: bucketArea

            anchors.fill: parent
            preventStealing: true
            containmentMask: QtObject {
                function contains(point: point): bool {
                    return root.polygonContains(bucketKey.modelData.outline, point)
                }
            }
            onPressed: {
                KeyboardHaptics.keyPress()
                root.keyTapped()
            }
            onReleased: {
                if (containsMouse)
                    root.activateGroupKey(bucketKey.modelData, bucketKey.modelData.value)
            }
        }
    }

    QtObject {
        id: shiftTaps
        property real last: 0
    }

    QtObject {
        id: word
        property string codes: ""
        property var candidates: []
        property string shown: ""
        property string accepted: ""
        property bool updating: false
    }

    Connections {
        target: root.buffer

        function onCompositionChanged() {
            if (!word.updating && word.codes !== "" && !root.buffer.composing)
                root.resetWord()
        }
    }

    Timer {
        id: commitTimer
        interval: Theme.keyboard.multiTapTimeout
        onTriggered: {
            if (root.buffer)
                root.buffer.commitComposition()
        }
    }

    Row {
        x: Theme.keyboard.padding
        y: Theme.keyboard.padding
        visible: root.predictionAvailable
        spacing: Theme.keyboard.keySpacing

        ListView {
            id: candidateList
            objectName: "t9Candidates"
            width: root.innerWidth - 2 * (root.sideWidth + Theme.keyboard.keySpacing)
            height: Theme.keyboard.candidateBarHeight
            orientation: ListView.Horizontal
            clip: true
            model: word.candidates

            delegate: Item {
                id: candidate

                required property string modelData
                required property int index

                objectName: "t9Candidate_" + index
                width: candidateText.implicitWidth + 2 * Theme.keyboard.candidatePadding
                height: candidateList.height

                Text {
                    id: candidateText
                    anchors.centerIn: parent
                    text: candidate.modelData
                    font.pixelSize: Theme.keyboard.candidateFontSize
                    font.bold: candidate.modelData === word.shown
                    color: candidate.modelData === word.shown
                           ? Theme.keyboard.selectedCandidateText
                           : Theme.keyboard.candidateText
                }

                Rectangle {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    visible: candidate.index < candidateList.count - 1
                    width: Theme.border.thin
                    height: parent.height / 2
                    color: Theme.keyboard.candidateSeparator
                }

                TapHandler {
                    onTapped: {
                        KeyboardHaptics.keyPress()
                        root.keyTapped()
                        root.chooseCandidate(candidate.index)
                    }
                }
            }
        }

        ThemedKeyboardKey {
            objectName: "t9KeyShape"
            width: root.sideWidth
            height: Theme.keyboard.candidateBarHeight
            special: true
            label: root.keyLayout === "qwerty" ? "3×3" : "qw"
            onTouched: root.keyTapped()
            onActivated: root.keyLayout = root.keyLayout === "qwerty" ? "phone" : "qwerty"
        }

        ThemedKeyboardKey {
            objectName: "t9KeyPredictive"
            width: root.sideWidth
            height: Theme.keyboard.candidateBarHeight
            special: true
            active: root.predictiveEnabled
            label: "T9"
            onTouched: root.keyTapped()
            onActivated: {
                root.acceptWord()
                root.predictiveEnabled = !root.predictiveEnabled
            }
        }
    }

    Row {
        x: Theme.keyboard.padding
        y: Theme.keyboard.padding + root.candidateBarSpace
        visible: !root.qwertyShape
        spacing: Theme.keyboard.keySpacing

        Grid {
            z: 1
            columns: 3
            columnSpacing: Theme.keyboard.keySpacing
            rowSpacing: Theme.keyboard.rowSpacing

            Repeater {
                model: root.digitKeys

                ThemedKeyboardKey {
                    required property var modelData

                    objectName: "t9Key_" + modelData.name
                    width: root.unit
                    height: root.keyHeight
                    label: modelData.code
                    hint: modelData.letters
                    value: modelData.value
                    showsPreview: false
                    popupCellWidth: root.qwertyUnit * Theme.keyboard.previewScale
                    popupHeight: root.qwertyKeyHeight * Theme.keyboard.previewScale
                    upperCase: root.upperCase
                    alternates: modelData.characters
                    onTouched: root.keyTapped()
                    onActivated: (value) => root.activateGroupKey(modelData, value)
                }
            }

            ThemedKeyboardKey {
                objectName: "t9KeyShift"
                width: root.unit
                height: root.keyHeight
                special: true
                enabled: !root.symbolsActive
                opacity: root.symbolsActive ? 0 : 1
                iconSource: Theme.keyboard.shiftIcon
                active: root.buffer !== null
                        && root.buffer.shiftState === KeyboardBuffer.ShiftState.Locked
                accented: root.buffer !== null
                          && root.buffer.shiftState === KeyboardBuffer.ShiftState.Once
                onTouched: root.keyTapped()
                onActivated: root.pressShift()
            }

            ThemedKeyboardKey {
                objectName: "t9KeySpace"
                width: root.unit
                height: root.keyHeight
                label: "0"
                hint: "␣"
                value: " "
                showsPreview: false
                alternates: ["0"]
                onTouched: root.keyTapped()
                onActivated: (value) => root.typeSpace(value)
            }

            ThemedKeyboardKey {
                objectName: "t9KeyLayer"
                width: root.unit
                height: root.keyHeight
                special: true
                label: root.symbolsActive ? root.languageLayout.layerLabel : "?123"
                onTouched: root.keyTapped()
                onActivated: root.toggleSymbols()
            }
        }

        Column {
            spacing: Theme.keyboard.rowSpacing

            ThemedKeyboardKey {
                objectName: "t9KeyBackspace"
                width: root.sideWidth
                height: root.tallKeyHeight
                special: true
                repeats: true
                iconSource: Theme.keyboard.backspaceIcon
                onTouched: root.keyTapped()
                onActivated: root.pressBackspace()
            }

            ThemedKeyboardKey {
                objectName: "t9KeyEnter"
                width: root.sideWidth
                height: root.tallKeyHeight
                special: true
                active: true
                label: root.enterLabel
                iconSource: root.enterLabel === "" ? Theme.keyboard.enterIcon : ""
                onTouched: root.keyTapped()
                onActivated: root.pressEnter()
            }
        }
    }

    Column {
        x: Theme.keyboard.padding
        y: Theme.keyboard.padding + root.candidateBarSpace
        width: root.innerWidth
        visible: root.qwertyShape
        spacing: Theme.keyboard.rowSpacing

        Item {
            width: root.innerWidth
            height: 3 * root.qwertyKeyHeight + 2 * Theme.keyboard.rowSpacing

            Repeater {
                model: root.bucketKeys

                BucketKey {}
            }

            ThemedKeyboardKey {
                objectName: "t9PairShift"
                x: root.letterCell(layouts.qwerty[2][0]).x - Theme.keyboard.keySpacing
                   - root.qwertySpecialWidth
                y: parent.height - height
                width: root.qwertySpecialWidth
                height: root.qwertyKeyHeight
                special: true
                iconSource: Theme.keyboard.shiftIcon
                active: root.buffer !== null
                        && root.buffer.shiftState === KeyboardBuffer.ShiftState.Locked
                accented: root.buffer !== null
                          && root.buffer.shiftState === KeyboardBuffer.ShiftState.Once
                onTouched: root.keyTapped()
                onActivated: root.pressShift()
            }

            ThemedKeyboardKey {
                objectName: "t9PairBackspace"
                x: root.letterCell(layouts.qwerty[2][layouts.qwerty[2].length - 1]).x
                   + root.qwertyUnit + Theme.keyboard.keySpacing
                y: parent.height - height
                width: root.qwertySpecialWidth
                height: root.qwertyKeyHeight
                special: true
                repeats: true
                iconSource: Theme.keyboard.backspaceIcon
                onTouched: root.keyTapped()
                onActivated: root.pressBackspace()
            }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.keyboard.keySpacing

            ThemedKeyboardKey {
                objectName: "t9PairLayer"
                width: root.qwertySpecialWidth
                height: root.qwertyKeyHeight
                special: true
                label: "?123"
                onTouched: root.keyTapped()
                onActivated: root.toggleSymbols()
            }

            ThemedKeyboardKey {
                objectName: "t9PairPunctuation"
                width: root.qwertyUnit
                height: root.qwertyKeyHeight
                label: root.punctuationKey.letters
                value: root.punctuationKey.value
                showsPreview: false
                alternates: root.punctuationKey.characters
                onTouched: root.keyTapped()
                onActivated: (value) => root.activateGroupKey(root.punctuationKey, value)
            }

            ThemedKeyboardKey {
                objectName: "t9PairSpace"
                width: root.innerWidth - 2 * root.qwertySpecialWidth - root.qwertyUnit
                       - 3 * Theme.keyboard.keySpacing
                height: root.qwertyKeyHeight
                value: " "
                showsPreview: false
                onTouched: root.keyTapped()
                onActivated: (value) => root.typeSpace(value)
            }

            ThemedKeyboardKey {
                objectName: "t9PairEnter"
                width: root.qwertySpecialWidth
                height: root.qwertyKeyHeight
                special: true
                active: true
                label: root.enterLabel
                iconSource: root.enterLabel === "" ? Theme.keyboard.enterIcon : ""
                onTouched: root.keyTapped()
                onActivated: root.pressEnter()
            }
        }
    }
}
