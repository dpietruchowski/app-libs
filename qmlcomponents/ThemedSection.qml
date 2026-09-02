import QtQuick
import QtQuick.Layouts

import Themed.Components

ColumnLayout {
    id: control

    default property alias sectionContent: body.data
    property string title: ""

    spacing: Theme.section.spacing

    ThemedText {
        Layout.fillWidth: true
        visible: control.title !== ""
        text: control.title
        textStyle: Theme.text.subtitle
    }

    ColumnLayout {
        id: body

        Layout.fillWidth: true
        Layout.fillHeight: true
        spacing: Theme.section.contentSpacing
    }
}
