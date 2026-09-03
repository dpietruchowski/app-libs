import QtQuick

import Themed.Components

Text {
    id: control

    property var textStyle: Theme.text.body

    color: control.textStyle.color
    font.pixelSize: control.textStyle.fontSize
    font.bold: control.textStyle.bold
    lineHeight: control.textStyle.lineHeight
    wrapMode: Text.WordWrap
}
