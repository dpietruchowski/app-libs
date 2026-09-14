import QtQuick
import QtQuick.Controls
import Themed.Components

TextEdit {
    id: root

    property string content: ""
    property bool centerAlign: true
    property int maxLines: 0
    property bool styled: false
    property int slotPosition: -1
    property int slotLength: 0
    property string revealedIcon: Theme.icons.eye

    readonly property string revealedImage: revealedIconProvider.svgSource === "" ? ""
        : '<img src="' + revealedIconProvider.svgSource + '" width="' + revealedIconProvider.width
          + '" height="' + revealedIconProvider.height + '" style="vertical-align: middle">'

    readonly property rect slotRect: {
        const start = rectAt(slotPosition)
        const end = rectAt(slotPosition >= 0 ? slotPosition + slotLength : -1)
        if (start.y + start.height > visibleHeight)
            return Qt.rect(0, 0, 0, 0)
        return Qt.rect(start.x, start.y, end.x - start.x, start.height)
    }

    readonly property real visibleHeight: maxLines > 0
                                          ? Math.min(implicitHeight, Math.ceil(lineMetrics.lineSpacing) * maxLines)
                                          : implicitHeight

    height: visibleHeight
    clip: visibleHeight < implicitHeight
    readOnly: true
    activeFocusOnPress: false
    selectByMouse: false
    selectByKeyboard: false
    cursorVisible: false
    textFormat: TextEdit.RichText
    wrapMode: TextEdit.WordWrap
    color: Theme.colors.textPrimary
    horizontalAlignment: centerAlign ? TextEdit.AlignHCenter : TextEdit.AlignLeft
    verticalAlignment: TextEdit.AlignTop
    text: styled ? buildStyled(content) : buildHtml(content)

    function buildStyled(content) {
        var errorColor = Theme.colors.error
        var successColor = Theme.colors.success
        var primaryColor = Theme.colors.primary

        return content
            .replace(/<\/?(?:p|h2|div|body|head|html)[^>]*>/g, "")
            .replace(/<span class="highlighted">([\s\S]*?)<\/span>/g,
                     '<b><font color="' + primaryColor + '">$1</font></b>')
            .replace(/<span class="correct">([\s\S]*?)<\/span>/g,
                     '<font color="' + successColor + '">$1</font>')
            .replace(/<span class="wrong">([\s\S]*?)<\/span>/g,
                     '<font color="' + errorColor + '"><s>$1</s></font>')
            .replace(/<span class="revealed"><\/span>/g, root.revealedImage)
            .trim()
    }

    function buildHtml(content) {
        var errorColor = Theme.colors.error
        var successColor = Theme.colors.success
        var primaryColor = Theme.colors.primary

        return `<html>
                    <head>
                        <style>
                            body {
                                margin: 0;
                                padding: 0;
                            }
                            p, h2 {
                                margin: 8px 0;
                            }
                            .wrong {
                                color: ${errorColor};
                                text-decoration: line-through;
                            }
                            .correct {
                                color: ${successColor};
                            }
                            .highlighted {
                                font-weight: bold;
                                color: ${primaryColor};
                            }
                        </style>
                    </head>
                    <body>
                        <div style="text-align: ${centerAlign ? 'center' : 'left'};">
                            ${content.replace(/<span class="revealed"><\/span>/g, root.revealedImage)}
                        </div>
                    </body>
                </html>`
    }

    FontMetrics {
        id: lineMetrics
        font: root.font
    }

    ColoredSvgProvider {
        id: revealedIconProvider
        svgOriginSource: root.revealedIcon
        color: Theme.colors.error
        width: Math.round(root.font.pixelSize > 0 ? root.font.pixelSize : lineMetrics.height)
        height: width
    }

    function rectAt(position) {
        root.text
        root.width
        root.contentWidth
        root.contentHeight
        return position >= 0 && position <= root.length ? root.positionToRectangle(position)
                                                        : Qt.rect(0, 0, 0, 0)
    }
}
