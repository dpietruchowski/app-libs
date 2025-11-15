import QtQuick
import QtQuick.Controls

import FillIn.Components

Item {
    id: root
    property alias svgSource: provider.svgOriginSource
    property alias color: provider.color
    width: 24
    height: 24

    ColoredSvgProvider {
        id: provider
        svgOriginSource: ""
        color: "black"
        width: root.width
        height: root.height
    }

    Image {
        anchors.fill: parent
        source: provider.svgSource
        visible: provider.svgSource && provider.svgSource !== ""
        fillMode: Image.PreserveAspectFit
        smooth: true
    }
}
