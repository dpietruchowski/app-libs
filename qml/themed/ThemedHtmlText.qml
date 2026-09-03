import QtQuick
import QtQuick.Controls
import Themed.Components

Text {
    id: root

    property string content: ""
    property bool centerAlign: true
    property int maxLines: 0
    property bool styled: false

    textFormat: styled ? Text.StyledText : Text.RichText
    wrapMode: Text.WordWrap
    maximumLineCount: maxLines > 0 ? maxLines : 1000000
    elide: maxLines > 0 ? Text.ElideRight : Text.ElideNone
    color: Theme.colors.textPrimary
    horizontalAlignment: centerAlign ? Text.AlignHCenter : Text.AlignLeft
    verticalAlignment: Text.AlignVCenter

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
                            ${content}
                        </div>
                    </body>
                </html>`
    }

    text: styled ? buildStyled(content) : buildHtml(content)

    onContentChanged: text = styled ? buildStyled(content) : buildHtml(content)
}
