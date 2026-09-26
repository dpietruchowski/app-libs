import QtQuick
import Themed.Components

Rectangle {
    id: caret

    width: 2
    color: Theme.colors.primary

    onVisibleChanged: opacity = 1

    SequentialAnimation on opacity {
        loops: Animation.Infinite
        running: caret.visible
        NumberAnimation { to: 0; duration: 80 }
        PauseAnimation { duration: 450 }
        NumberAnimation { to: 1; duration: 80 }
        PauseAnimation { duration: 450 }
    }
}
