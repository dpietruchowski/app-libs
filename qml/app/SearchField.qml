import QtQuick
import QtQuick.Controls
import Themed.Components

TextField {
    id: root

    property int searchDelay: 0

    signal searchRequested(string text)

    onTextChanged: {
        if (root.searchDelay > 0)
        {
            debounceTimer.restart()
        }
        else
        {
            root.searchRequested(root.text)
        }
    }

    Timer {
        id: debounceTimer
        interval: root.searchDelay
        onTriggered: root.searchRequested(root.text)
    }

    placeholderText: qsTr("Search...")
    placeholderTextColor: Theme.colors.textPlaceholder
    color: Theme.colors.textPrimary
    font.pixelSize: Theme.fontSize.medium
    leftPadding: Theme.padding.medium * 2 + Theme.icon.small
    rightPadding: Theme.padding.medium
    implicitHeight: Theme.input.searchHeight
    inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase | Qt.ImhSensitiveData

    background: Rectangle {
        color: Theme.colors.cardBackground
        border.color: root.activeFocus ? Theme.colors.primary : Theme.colors.cardBorder
        border.width: root.activeFocus ? Theme.border.medium : Theme.border.thin
        radius: Theme.radius.xLarge

        ThemedIcon {
            anchors.left: parent.left
            anchors.leftMargin: Theme.padding.medium
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.icon.small
            height: Theme.icon.small
            svgSource: Theme.icons.search
            color: Theme.colors.textPlaceholder
        }
    }
}
