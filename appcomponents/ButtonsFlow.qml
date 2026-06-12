import QtQuick
import QtQuick.Controls
import Themed.Components

Flow {
    id: flow
    x: leftMar
    y: topMar
    width: parent.width - 2 * leftMar

    property var model: []
    property alias count: elements.count
    signal buttonClicked(string text)

    property int rowCount: 0
    property int rowWidth: 0
    property int nRows: 0
    property int rowHeight: 0
    property int leftMar: Math.max(0, (parent.width - rowWidth) / 2)
    property int topMar: Math.max(0, (parent.height - rowHeight) / 2)

    spacing: Theme.spacing.small

    function calculateRowCount() {
        if (elements.count === 0 || !elements.itemAt(0)) return 0
        return Math.floor(parent.width / (elements.itemAt(0).width + spacing))
    }

    function calculateRowWidth() {
        if (elements.count === 0 || !elements.itemAt(0)) return 0
        return Math.min(rowCount, elements.count) * elements.itemAt(0).width + (rowCount - 1) * spacing
    }

    function calculateNRows() {
        if (rowCount === 0) return 0
        return Math.ceil(elements.count / rowCount)
    }

    function calculateRowHeight() {
        if (elements.count === 0 || !elements.itemAt(0)) return 0
        return nRows * elements.itemAt(0).height + (nRows - 1) * spacing
    }

    function recalculateMargins() {
        rowCount = calculateRowCount()
        rowWidth = calculateRowWidth()
        nRows = calculateNRows()
        rowHeight = calculateRowHeight()
    }

    Connections {
        target: flow.parent
        function onWidthChanged() { flow.recalculateMargins() }
        function onHeightChanged() { flow.recalculateMargins() }
    }

    Repeater {
        id: elements
        model: flow.model

        ThemedButton {
            text: modelData.text !== undefined ? modelData.text : modelData
            buttonSize: Theme.button.medium
            buttonStyle: modelData.style !== undefined ? modelData.style : Theme.button.secondary
            onClicked: flow.buttonClicked(text)
        }

        onCountChanged: flow.recalculateMargins()
    }
}
