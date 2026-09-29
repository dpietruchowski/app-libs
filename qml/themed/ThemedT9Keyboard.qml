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

    readonly property var groups: symbolsActive ? symbolGroups : letterGroups
    readonly property var digitKeys: [root.punctuation].concat(root.groups).map((group, index) => ({
        digit: String(index + 1),
        hint: group,
        value: group + String(index + 1),
        characters: Array.from(group + String(index + 1))
    }))
    readonly property real innerWidth: width - 2 * Theme.keyboard.padding
    readonly property real unit: (innerWidth - 3 * Theme.keyboard.keySpacing)
                                 / (3 + Theme.keyboard.t9SideKeyUnits)
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

    function showWord(text) {
        word.updating = true
        root.buffer.composeWord(text)
        word.updating = false
        word.shown = text
    }

    function lookUpCandidates() {
        word.candidates = root.predictor.candidates(word.digits, root.candidateLimit)
    }

    function typeDigit(digit, letters) {
        if (!root.buffer)
            return
        var previous = word.shown
        word.digits += digit
        root.lookUpCandidates()
        root.showWord(word.candidates.length > 0
                      ? word.candidates[0].substring(0, word.digits.length)
                      : previous + letters.charAt(0))
    }

    function eraseDigit() {
        word.digits = word.digits.slice(0, -1)
        if (word.digits === "") {
            root.showWord("")
            root.resetWord()
            return
        }
        var previous = word.shown
        root.lookUpCandidates()
        root.showWord(word.candidates.length > 0
                      ? word.candidates[0].substring(0, word.digits.length)
                      : previous.slice(0, -1))
    }

    function chooseCandidate(index) {
        root.showWord(word.candidates[index])
        root.acceptWord()
    }

    function acceptWord() {
        if (word.digits === "")
            return
        var accepted = word.shown
        var known = word.candidates.indexOf(accepted) >= 0
        root.resetWord()
        root.buffer.commitComposition()
        if (known)
            root.predictor.learn(accepted)
    }

    function learnTypedWord() {
        if (!root.predictionAvailable || root.predictiveEnabled || !root.buffer)
            return
        var typed = root.buffer.text.substring(0, root.buffer.cursorPosition).match(/[A-Za-z]+$/)
        if (typed)
            root.predictor.learn(typed[0])
    }

    function finishWord() {
        root.acceptWord()
        root.learnTypedWord()
    }

    function resetWord() {
        word.digits = ""
        word.candidates = []
        word.shown = ""
    }

    objectName: "t9Keyboard"
    implicitWidth: Theme.applicationWidth
    implicitHeight: candidateBarSpace + 4 * keyHeight + 3 * Theme.keyboard.rowSpacing
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
        property string digits: ""
        property var candidates: []
        property string shown: ""
        property bool updating: false
    }

    Connections {
        target: root.buffer

        function onCompositionChanged() {
            if (!word.updating && word.digits !== "" && !root.buffer.composing)
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
            width: root.innerWidth - root.sideWidth - Theme.keyboard.keySpacing
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
            columns: 3
            columnSpacing: Theme.keyboard.keySpacing
            rowSpacing: Theme.keyboard.rowSpacing

            Repeater {
                model: root.digitKeys

                ThemedKeyboardKey {
                    required property var modelData
                    required property int index

                    objectName: "t9Key_" + modelData.digit
                    width: root.unit
                    height: root.keyHeight
                    label: modelData.digit
                    hint: modelData.hint
                    value: modelData.value
                    showsPreview: false
                    upperCase: root.upperCase
                    alternates: modelData.characters
                    onTouched: root.keyTapped()
                    onActivated: (value) => {
                        if (value === modelData.value && root.predicting && index > 0) {
                            root.typeDigit(modelData.digit, modelData.hint)
                            return
                        }
                        root.acceptWord()
                        if (value === modelData.value)
                            root.compose(modelData.characters)
                        else
                            root.typeKey(value)
                    }
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
                onActivated: {
                    root.acceptWord()
                    root.tapShift()
                }
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
                onActivated: (value) => {
                    if (!root.buffer)
                        return
                    root.finishWord()
                    root.buffer.insert(value)
                }
            }

            ThemedKeyboardKey {
                objectName: "t9KeyLayer"
                width: root.unit
                height: root.keyHeight
                special: true
                label: root.symbolsActive ? root.languageLayout.layerLabel : "?123"
                onTouched: root.keyTapped()
                onActivated: {
                    root.acceptWord()
                    if (root.buffer)
                        root.buffer.commitComposition()
                    root.symbolsActive = !root.symbolsActive
                }
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
                onActivated: {
                    if (!root.buffer)
                        return
                    if (word.digits !== "")
                        root.eraseDigit()
                    else
                        root.buffer.backspace()
                }
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
                onActivated: {
                    if (!root.buffer)
                        return
                    root.finishWord()
                    root.buffer.submit()
                }
            }
        }
    }
}
