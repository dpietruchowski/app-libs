import QtQuick
import QtQuick.Layouts

import Themed.Components

ColumnLayout {
    id: root

    property var steps: []
    property bool expanded: false

    readonly property bool failed: root.steps.some(function (step) {
        return step.status === "failed"
    })

    function summarize(steps) {
        var parts = []
        var positions = {}
        var group = 0
        for (var i = 0; i < steps.length; ++i) {
            var step = steps[i]
            if (step.kind === "delegation") {
                group += 1
                parts.push({ label: qsTr("Asked **%1**").arg(step.agentTitle), count: 1 })
                continue
            }
            var key = group + ":" + step.tool
            if (positions[key] !== undefined) {
                parts[positions[key]].count += 1
                continue
            }
            positions[key] = parts.length
            parts.push({
                label: "`" + step.tool + "`" + (step.effect === "writes" ? " ✎" : ""),
                count: 1
            })
        }
        return parts.map(function (part) {
            return part.count > 1 ? part.label + " ×" + part.count : part.label
        }).join(" · ")
    }

    function stepLabel(step) {
        if (step.kind === "delegation")
            return qsTr("Asked %1").arg(step.agentTitle)
        return step.effect === "writes" ? step.tool + " ✎" : step.tool
    }

    function stepColor(step) {
        if (step.status === "failed")
            return Theme.chat.activityFailed
        if (step.effect === "writes")
            return Theme.chat.activityWrite
        return Theme.chat.activityText
    }

    visible: root.steps.length > 0
    spacing: Theme.chat.activitySpacing

    Item {
        objectName: root.objectName !== "" ? root.objectName + "Toggle" : ""
        Layout.fillWidth: true
        implicitHeight: header.implicitHeight + Theme.chat.activitySpacing * 2
        Accessible.role: Accessible.Button
        Accessible.name: root.expanded ? qsTr("Hide steps") : qsTr("Show steps")
        Accessible.onPressAction: root.expanded = !root.expanded

        RowLayout {
            id: header

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.chat.activitySpacing

            ThemedIcon {
                Layout.preferredWidth: Theme.chat.activityIconSize
                Layout.preferredHeight: Theme.chat.activityIconSize
                svgSource: root.expanded ? Theme.icons.chevronDown : Theme.icons.chevronRight
                color: root.failed ? Theme.chat.activityFailed : Theme.chat.activityText
            }

            Text {
                objectName: root.objectName !== "" ? root.objectName + "Summary" : ""
                Layout.fillWidth: true
                text: root.summarize(root.steps)
                textFormat: Text.MarkdownText
                elide: Text.ElideRight
                clip: true
                maximumLineCount: 1
                color: root.failed ? Theme.chat.activityFailed : Theme.chat.activityText
                font.pixelSize: Theme.chat.activityFontSize
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: root.expanded = !root.expanded
        }
    }

    Repeater {
        model: root.expanded ? root.steps : []

        RowLayout {
            id: row

            required property var modelData
            required property int index

            objectName: root.objectName !== "" ? root.objectName + "Step" + row.index : ""
            Layout.fillWidth: true
            Layout.leftMargin: row.modelData.parentId !== "" ? Theme.chat.activityIndent : 0
            spacing: Theme.chat.activitySpacing

            Item {
                Layout.preferredWidth: Theme.chat.activityIconSize
                Layout.preferredHeight: Theme.chat.activityIconSize

                ThemedBusy {
                    anchors.fill: parent
                    running: row.modelData.status === "running"
                    size: Theme.chat.activityIconSize
                    dotSize: Theme.busy.dotSize * Theme.chat.activityIconSize / Theme.busy.size
                }

                ThemedIcon {
                    anchors.fill: parent
                    visible: row.modelData.status !== "running"
                    svgSource: row.modelData.status === "failed" ? Theme.icons.error
                                                                 : Theme.icons.check
                    color: root.stepColor(row.modelData)
                }
            }

            Text {
                Layout.fillWidth: true
                text: root.stepLabel(row.modelData)
                elide: Text.ElideRight
                color: root.stepColor(row.modelData)
                font.pixelSize: Theme.chat.activityFontSize
            }
        }
    }
}
