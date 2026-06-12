import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Themed.Components

Item {
    id: root

    property var cardsModel
    property bool isProcessing: false

    readonly property int fixedColumns: 3
    readonly property int fixedRows: 3
    readonly property int totalSlots: fixedColumns * fixedRows
    
    property int cardCount: 0
    property int emptySlots: Math.max(0, totalSlots - cardCount)

    function updateCardCount() {
        cardCount = cardsModel ? cardsModel.rowCount() : 0
    }

    onCardsModelChanged: updateCardCount()

    Connections {
        target: cardsModel
        function onModelReset() {
            root.updateCardCount()
        }
        function onRowsInserted() {
            root.updateCardCount()
        }
        function onRowsRemoved() {
            root.updateCardCount()
        }
    }

    property real cardWidth: fixedColumns > 0 ? (gridLayout.width - (fixedColumns - 1) * gridLayout.columnSpacing) / fixedColumns : 0
    property real cardHeight: fixedRows > 0 ? (gridLayout.height - (fixedRows - 1) * gridLayout.rowSpacing) / fixedRows : 0

    GridLayout {
        id: gridLayout
        anchors.fill: parent

        columns: root.fixedColumns
        columnSpacing: Theme.spacing.medium
        rowSpacing: Theme.spacing.medium

        Repeater {
            model: root.cardsModel

            delegate: Item {
                Layout.preferredWidth: root.cardWidth
                Layout.preferredHeight: root.cardHeight
                
                property bool modelFlipped: model.isFlipped
                
                FlipCard {
                    id: card
                    anchors.fill: parent
                    textSize: Theme.fontSize.normal
                    sourceText: ""
                    targetText: model.expression
                    enabled: !model.isDisabled
                }
                
                onModelFlippedChanged: {
                    if (modelFlipped !== card.flipped) {
                        card.flip()
                    }
                }
                
                MouseArea {
                    anchors.fill: parent
                    preventStealing: true
                    propagateComposedEvents: false
                    onClicked: {
                        if (!root.isProcessing && !model.isDisabled && !model.isFlipped) {
                            root.cardsModel.flipCard(index)
                        }
                    }
                }
            }
        }

        Repeater {
            model: root.emptySlots

            delegate: FlipCard {
                Layout.preferredWidth: root.cardWidth
                Layout.preferredHeight: root.cardHeight
                textSize: Theme.fontSize.normal
                enabled: false
                sourceText: ""
                targetText: ""
            }
        }
    }
}
