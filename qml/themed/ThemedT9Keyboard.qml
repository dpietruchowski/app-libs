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
    property string joiners: "'-"

    readonly property var groups: symbolsActive ? symbolGroups : letterGroups
    readonly property var digitKeys: [root.punctuation].concat(root.groups).map((group, index) => ({
        name: String(index + 1),
        code: String(index + 1),
        letters: group,
        value: group + String(index + 1),
        characters: Array.from(group + String(index + 1)),
        predictive: index > 0
    }))
    readonly property var keyMap: root.digitKeys
        .reduce((map, key) => Object.assign(map, {
            [key.code]: key.predictive ? key.letters : root.joiners
        }), {})
    readonly property real innerWidth: width - 2 * Theme.keyboard.padding
    readonly property real unit: (innerWidth - 3 * Theme.keyboard.keySpacing)
                                 / (3 + Theme.keyboard.t9SideKeyUnits)
    readonly property real popupUnit: (innerWidth - 9 * Theme.keyboard.keySpacing) / 10
    readonly property real sideWidth: unit * Theme.keyboard.t9SideKeyUnits
    readonly property real keyHeight: Theme.keyboard.t9KeyHeight
    readonly property real tallKeyHeight: 2 * keyHeight + Theme.keyboard.rowSpacing
    readonly property bool upperCase: buffer !== null && buffer.upperCase && !symbolsActive
    readonly property bool predictionAvailable: predictor !== null
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

    function showWord(text) {
        word.updating = true
        root.buffer.composeWord(text)
        word.updating = false
        word.shown = text
    }

    function lookUpCandidates() {
        var groups = Array.from(word.codes).map(code => root.keyMap[code])
        word.candidates = root.predictor.candidatesForGroups(layouts.codeFor(root.language), groups,
                                                             root.candidateLimit)
    }

    function typeJoiner(code) {
        word.codes += code
        root.lookUpCandidates()
        if (word.candidates.length > 0) {
            root.showWord(word.candidates[0].substring(0, word.codes.length))
            return true
        }
        word.codes = word.codes.slice(0, -1)
        root.lookUpCandidates()
        return false
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
            root.predictor.learn(layouts.codeFor(root.language), accepted)
            word.accepted = accepted
        }
    }

    function learnTypedWord() {
        if (!root.predictionAvailable || !root.buffer)
            return
        var typed = root.trailingWord(root.buffer.text.substring(0, root.buffer.cursorPosition))
        if (typed !== "" && typed !== word.accepted)
            root.predictor.learn(layouts.codeFor(root.language), typed)
    }

    function isLetter(character) {
        return character.toLowerCase() !== character.toUpperCase()
    }

    function trailingWord(text) {
        var start = text.length
        while (start > 0) {
            var character = text.charAt(start - 1)
            if (root.isLetter(character))
                start--
            else if ("'’-".includes(character) && start < text.length && start > 1
                     && root.isLetter(text.charAt(start - 2)))
                start--
            else
                break
        }
        return text.substring(start)
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
        if (value === key.value && root.predicting && word.codes !== "" && root.typeJoiner(key.code))
            return
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
    implicitHeight: candidateBarSpace + 4 * keyHeight
                    + 3 * Theme.keyboard.rowSpacing
                    + 2 * Theme.keyboard.padding
    color: Theme.keyboard.background

    onPredictingChanged: {
        if (!root.predicting)
            root.acceptWord()
    }

    ThemedKeyboardLayouts {
        id: layouts
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
                    popupCellWidth: root.popupUnit * Theme.keyboard.previewScale
                    popupHeight: Theme.keyboard.keyHeight * Theme.keyboard.previewScale
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
}
