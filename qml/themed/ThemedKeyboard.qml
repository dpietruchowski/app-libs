import QtQuick

import Themed.Components

Rectangle {
    id: root

    property KeyboardBuffer buffer
    property string language: ""
    readonly property var languageLayout: layouts.layoutFor(language)
    property var letterRows: languageLayout.letters
    property var symbolRows: layouts.symbols
    property var alternates: languageLayout.alternates
    property string enterLabel: ""
    property bool symbolsActive: false

    readonly property var rows: symbolsActive ? symbolRows : letterRows
    readonly property var edgeRow: rows.length > 0 ? rows[rows.length - 1] : []
    readonly property int columns: Math.max(1, ...letterRows.concat(symbolRows).map(row => row.length))
    readonly property real innerWidth: width - 2 * Theme.keyboard.padding
    readonly property real unit: (innerWidth - (columns - 1) * Theme.keyboard.keySpacing) / columns
    readonly property real specialWidth: unit * Theme.keyboard.specialKeyUnits
    readonly property bool upperCase: buffer !== null && buffer.upperCase && !symbolsActive

    signal keyTapped()

    function alternatesFor(key) {
        return root.alternates[key] ?? []
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

    objectName: "keyboard"
    implicitWidth: Theme.applicationWidth
    implicitHeight: keyRows.implicitHeight + 2 * Theme.keyboard.padding
    color: Theme.keyboard.background

    ThemedKeyboardLayouts {
        id: layouts
    }

    QtObject {
        id: shiftTaps
        property real last: 0
    }

    component CharacterKey: ThemedKeyboardKey {
        required property string modelData

        objectName: "keyboardKey_" + modelData
        label: modelData
        width: root.unit
        upperCase: root.upperCase
        alternates: root.alternatesFor(modelData)
        onTouched: root.keyTapped()
        onActivated: (value) => root.typeKey(value)
    }

    Column {
        id: keyRows

        x: Theme.keyboard.padding
        y: Theme.keyboard.padding
        width: root.innerWidth
        spacing: Theme.keyboard.rowSpacing

        Repeater {
            model: Math.max(0, root.rows.length - 1)

            Row {
                required property int index

                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.keyboard.keySpacing

                Repeater {
                    model: root.rows[parent.index]

                    CharacterKey {}
                }
            }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.keyboard.keySpacing

            ThemedKeyboardKey {
                objectName: "keyboardKeyShift"
                width: root.specialWidth
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

            Repeater {
                model: root.edgeRow

                CharacterKey {}
            }

            ThemedKeyboardKey {
                objectName: "keyboardKeyBackspace"
                width: root.specialWidth
                special: true
                repeats: true
                iconSource: Theme.keyboard.backspaceIcon
                onTouched: root.keyTapped()
                onActivated: {
                    if (root.buffer)
                        root.buffer.backspace()
                }
            }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: Theme.keyboard.keySpacing

            ThemedKeyboardKey {
                objectName: "keyboardKeyLayer"
                width: root.specialWidth
                special: true
                label: root.symbolsActive ? root.languageLayout.layerLabel : "?123"
                onTouched: root.keyTapped()
                onActivated: root.symbolsActive = !root.symbolsActive
            }

            ThemedKeyboardKey {
                objectName: "keyboardKeySpace"
                width: root.innerWidth - 2 * root.specialWidth - root.unit
                       - 3 * Theme.keyboard.keySpacing
                value: " "
                showsPreview: false
                onTouched: root.keyTapped()
                onActivated: {
                    if (root.buffer)
                        root.buffer.insert(" ")
                }
            }

            CharacterKey {
                modelData: "'"
            }

            ThemedKeyboardKey {
                objectName: "keyboardKeyEnter"
                width: root.specialWidth
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
