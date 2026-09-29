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

    objectName: "t9Keyboard"
    implicitWidth: Theme.applicationWidth
    implicitHeight: 4 * keyHeight + 3 * Theme.keyboard.rowSpacing + 2 * Theme.keyboard.padding
    color: Theme.keyboard.background

    ThemedKeyboardLayouts {
        id: layouts
    }

    QtObject {
        id: shiftTaps
        property real last: 0
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
        spacing: Theme.keyboard.keySpacing

        Grid {
            columns: 3
            columnSpacing: Theme.keyboard.keySpacing
            rowSpacing: Theme.keyboard.rowSpacing

            Repeater {
                model: root.digitKeys

                ThemedKeyboardKey {
                    required property var modelData

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
                onActivated: root.tapShift()
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
                    if (root.buffer)
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
                    if (root.buffer)
                        root.buffer.submit()
                }
            }
        }
    }
}
