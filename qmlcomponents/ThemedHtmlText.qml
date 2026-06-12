import QtQuick
import QtQuick.Controls
import Themed.Components

Text {
    id: root

    property string content: ""
    property bool centerAlign: true

    textFormat: Text.RichText
    wrapMode: Text.WordWrap
    color: Theme.colors.textPrimary
    horizontalAlignment: centerAlign ? Text.AlignHCenter : Text.AlignLeft
    verticalAlignment: Text.AlignVCenter

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

    text: buildHtml(content)

    onContentChanged: text = buildHtml(content)
}
