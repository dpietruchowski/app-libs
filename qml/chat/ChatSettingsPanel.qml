import QtQuick
import QtQuick.Layouts

import Themed.Components

ThemedPanel {
    id: root

    objectName: "chatSettingsPanel"

    property string model: ""
    property string reasoningEffort: ""
    property var agents: []
    readonly property var models: ["haiku", "sonnet", "opus"]
    readonly property var reasoningEfforts: ["Minimal", "Low", "Medium", "High"]

    signal modelChosen(string model)
    signal agentModelChosen(string key, string model)
    signal reasoningEffortChosen(string reasoningEffort)

    component ChoiceRow: RowLayout {
        id: choiceRow

        property string namePrefix
        property var choices: []
        property string current
        signal chosen(string choice)

        Layout.fillWidth: true
        spacing: Theme.spacing.small

        Repeater {
            model: choiceRow.choices

            ThemedButton {
                required property string modelData

                objectName: choiceRow.namePrefix + modelData
                Layout.fillWidth: true
                buttonSize: Theme.button.small
                buttonStyle: modelData === choiceRow.current ? Theme.button.primary
                                                             : Theme.button.ghost
                pill: true
                text: modelData.charAt(0).toUpperCase() + modelData.slice(1)
                onClicked: choiceRow.chosen(modelData)
            }
        }
    }

    panelTitle: qsTr("Chat settings")
    contentSpacing: Theme.spacing.medium

    ThemedOverline {
        text: qsTr("Model")
    }

    ChoiceRow {
        namePrefix: "chatModel"
        choices: root.models
        current: root.model
        onChosen: (choice) => root.modelChosen(choice)
    }

    Repeater {
        model: root.agents

        ColumnLayout {
            id: agentModel

            required property var modelData

            Layout.fillWidth: true
            spacing: Theme.spacing.small

            ThemedOverline {
                Layout.topMargin: Theme.spacing.small
                text: qsTr("%1 model").arg(agentModel.modelData.title)
            }

            ChoiceRow {
                namePrefix: "chatAgentModel_" + agentModel.modelData.key + "_"
                choices: root.models
                current: agentModel.modelData.model
                onChosen: (choice) => root.agentModelChosen(agentModel.modelData.key, choice)
            }
        }
    }

    ThemedOverline {
        Layout.topMargin: Theme.spacing.small
        text: qsTr("Reasoning")
    }

    ChoiceRow {
        namePrefix: "chatReasoning"
        choices: root.reasoningEfforts
        current: root.reasoningEffort
        onChosen: (choice) => root.reasoningEffortChosen(choice)
    }
}
