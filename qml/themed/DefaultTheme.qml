import QtQuick

QtObject {
    id: theme

    property bool isNightMode: false

    property int applicationWidth: 360
    property int applicationHeight: 640

    readonly property bool isLandscape: applicationWidth > applicationHeight

    readonly property real scale: {
        if (isLandscape)
            return Math.max(1.0, Math.min(applicationHeight / 683, 1.5))
        var minSide = Math.min(applicationWidth, applicationHeight)
        return Math.max(1.0, Math.min(minSide / 500, 1.5))
    }
    function scaled(value) { return Math.round(value * scale) }

    property int contentMaxWidthBase: 460
    readonly property int contentMaxWidth: scale > 1 ? scaled(contentMaxWidthBase) : 400
    readonly property int contentWidth: Math.min(applicationWidth, contentMaxWidth)

    property color systemBarBackground: isNightMode ? colors.backgroundDark : colors.backgroundLight

    property QtObject colors: QtObject {
        readonly property color backgroundDark: "#1A1A1A"
        readonly property color backgroundLight: "#FFFFFF"
        readonly property color background: theme.isNightMode ? backgroundDark : backgroundLight
        readonly property color surface: theme.isNightMode ? "#242424" : "#F0F1F3"
        readonly property color dialogSurface: theme.isNightMode ? "#2D2D2D" : "#FFFFFF"

        readonly property color primary: theme.isNightMode ? "#5B9AF6" : "#3B82F6"
        readonly property color primaryDark: theme.isNightMode ? "#4080E6" : "#2563EB"
        readonly property color primaryVariant: theme.isNightMode ? "#70AFFA" : "#60A5FA"
        readonly property color secondary: theme.isNightMode ? "#A78BFA" : "#8B5CF6"
        readonly property color accent: theme.isNightMode ? "#FFA826" : "#F59E0B"

        readonly property color info: theme.isNightMode ? "#5B9AF6" : "#3B82F6"
        readonly property color success: theme.isNightMode ? "#20C991" : "#10B981"
        readonly property color warning: theme.isNightMode ? "#FFA826" : "#F59E0B"
        readonly property color error: theme.isNightMode ? "#F87171" : "#EF4444"

        readonly property color textPrimary: theme.isNightMode ? "#E5E5E5" : "#1F2937"
        readonly property color textSecondary: theme.isNightMode ? "#A0A0A0" : "#5B616B"
        readonly property color textDisabled: theme.isNightMode ? "#808080" : "#9CA3AF"
        readonly property color textPlaceholder: theme.isNightMode ? "#707070" : "#6B7280"
        readonly property color textInverse: theme.isNightMode ? "#1F2937" : "#FFFFFF"

        readonly property color border: theme.isNightMode ? "#3A3A3A" : "#C5C6CA"
        readonly property color borderStrong: theme.isNightMode ? "#505050" : "#A0A3A8"
        readonly property color divider: theme.isNightMode ? "#2D2D2D" : "#C8C9CE"
        readonly property color overlay: theme.isNightMode ? "#000000B0" : "#00000080"
        readonly property color overlayLight: theme.isNightMode ? "#A6000000" : "#66000000"

        readonly property color cardBackground: theme.isNightMode ? "#242424" : "#F0F1F3"
        readonly property color cardBorder: theme.isNightMode ? "#3A3A3A" : "#C5C6CA"
        readonly property color cardBorderHover: theme.isNightMode ? "#505050" : "#A0A3A8"
    }

    property QtObject fontSize: QtObject {
        readonly property int huge: theme.scaled(72)
        readonly property int xxLarge: theme.scaled(32)
        readonly property int xLarge: theme.scaled(24)
        readonly property int large: theme.scaled(20)
        readonly property int xMedium: theme.scaled(18)
        readonly property int medium: theme.scaled(16)
        readonly property int normal: theme.scaled(14)
        readonly property int small: theme.scaled(12)
        readonly property int xSmall: theme.scaled(10)
    }

    property QtObject spacing: QtObject {
        readonly property int xSmall: theme.scaled(4)
        readonly property int small: theme.scaled(8)
        readonly property int medium: theme.scaled(12)
        readonly property int large: theme.scaled(16)
        readonly property int xLarge: theme.scaled(24)
        readonly property int xxLarge: theme.scaled(32)
    }

    property QtObject padding: QtObject {
        readonly property int xSmall: theme.scaled(4)
        readonly property int small: theme.scaled(8)
        readonly property int medium: theme.scaled(16)
        readonly property int large: theme.scaled(24)
        readonly property int xLarge: theme.scaled(32)
    }

    property QtObject radius: QtObject {
        readonly property int small: 4
        readonly property int medium: 8
        readonly property int large: 12
        readonly property int xLarge: 16
        readonly property int full: 9999
    }

    property QtObject border: QtObject {
        readonly property int thin: 1
        readonly property int medium: 2
        readonly property int thick: 3
    }

    property QtObject opacity: QtObject {
        readonly property real dialog: 1.0
        readonly property real card: 1.0
        readonly property real disabled: 0.5
        readonly property real hover: 0.08
    }

    property QtObject elevation: QtObject {
        readonly property color shadowColor: theme.isNightMode ? "#000000" : "#1E293B"
        readonly property real shadowLayerOpacity: theme.isNightMode ? 0.09 : 0.04
        readonly property int shadowBlur: theme.scaled(10)
        readonly property int shadowOffset: theme.scaled(2)
        readonly property int shadowSteps: 6
    }

    property QtObject text: QtObject {
        readonly property QtObject body: QtObject {
            readonly property color color: theme.colors.textPrimary
            readonly property int fontSize: theme.fontSize.normal
            readonly property bool bold: false
            readonly property real lineHeight: 1.2
        }

        readonly property QtObject subtitle: QtObject {
            readonly property color color: theme.colors.textPrimary
            readonly property int fontSize: theme.fontSize.medium
            readonly property bool bold: true
            readonly property real lineHeight: 1.2
        }

        readonly property QtObject caption: QtObject {
            readonly property color color: theme.colors.textSecondary
            readonly property int fontSize: theme.fontSize.small
            readonly property bool bold: false
            readonly property real lineHeight: 1.2
        }

        readonly property QtObject muted: QtObject {
            readonly property color color: theme.colors.textDisabled
            readonly property int fontSize: theme.fontSize.normal
            readonly property bool bold: false
            readonly property real lineHeight: 1.2
        }
    }

    property QtObject page: QtObject {
        readonly property QtObject title: QtObject {
            readonly property color color: theme.colors.textPrimary
            readonly property int fontSize: theme.fontSize.large
            readonly property bool bold: true
            readonly property real lineHeight: 1.2
        }
    }

    property QtObject busy: QtObject {
        readonly property int size: theme.scaled(48)
        readonly property int dotCount: 8
        readonly property int dotSize: theme.scaled(6)
        readonly property int duration: 1000
        readonly property color color: theme.colors.primary
    }

    property QtObject empty: QtObject {
        readonly property int maxWidth: theme.scaled(280)
        readonly property int spacing: theme.spacing.medium
        readonly property int iconSize: theme.scaled(48)
        readonly property color iconColor: theme.colors.textDisabled
    }

    property QtObject grid: QtObject {
        readonly property int columns: theme.isLandscape ? 3 : 2
    }

    property QtObject list: QtObject {
        readonly property int spacing: theme.spacing.small
        readonly property int itemHeight: theme.scaled(56)
        readonly property int itemPadding: theme.padding.small
        readonly property int lineSpacing: theme.spacing.xSmall
        readonly property int iconSize: theme.scaled(24)
        readonly property color iconColor: theme.colors.textSecondary
    }

    property QtObject scroll: QtObject {
        readonly property int barWidth: theme.scaled(4)
        readonly property int barRadius: theme.scaled(2)
        readonly property color barColor: theme.colors.borderStrong
        readonly property real barIdleOpacity: 0.35
    }

    property QtObject section: QtObject {
        readonly property int spacing: theme.spacing.medium
        readonly property int contentSpacing: theme.spacing.small
    }

    property QtObject separator: QtObject {
        readonly property int thickness: theme.border.thin
        readonly property color color: theme.colors.divider
    }

    property QtObject slider: QtObject {
        readonly property int width: theme.contentWidth * 0.6
        readonly property int handleSize: theme.scaled(20)
        readonly property int trackHeight: theme.scaled(4)
        readonly property color trackColor: theme.colors.border
        readonly property color fillColor: theme.colors.primary
        readonly property color handleColor: theme.colors.background
        readonly property color handlePressedColor: theme.colors.primaryVariant
    }

    property QtObject button: QtObject {
        readonly property int radius: theme.radius.xLarge

        readonly property QtObject square: QtObject {
            readonly property int size: theme.scaled(32)
            readonly property int width: size
            readonly property int height: size
            readonly property int fontSize: theme.fontSize.small
            readonly property int iconSize: theme.scaled(18)
        }

        readonly property QtObject icon: QtObject {
            readonly property int size: theme.scaled(44)
            readonly property int width: size
            readonly property int height: size
            readonly property int fontSize: theme.fontSize.medium
            readonly property int iconSize: theme.scaled(24)
        }

        readonly property QtObject iconSmall: QtObject {
            readonly property int size: theme.scaled(33)
            readonly property int width: size
            readonly property int height: size
            readonly property int fontSize: theme.fontSize.normal
            readonly property int iconSize: theme.scaled(16)
        }

        readonly property QtObject small: QtObject {
            readonly property int width: theme.contentWidth * 0.20
            readonly property int height: theme.scaled(36)
            readonly property int fontSize: theme.fontSize.small
            readonly property int iconSize: theme.scaled(16)
        }

        readonly property QtObject compact: QtObject {
            readonly property int width: theme.contentWidth * 0.24
            readonly property int height: theme.scaled(40)
            readonly property int fontSize: theme.fontSize.normal
            readonly property int iconSize: theme.scaled(18)
        }

        readonly property QtObject medium: QtObject {
            readonly property int width: theme.contentWidth * 0.30
            readonly property int height: theme.scaled(44)
            readonly property int fontSize: theme.fontSize.medium
            readonly property int iconSize: theme.scaled(20)
        }

        readonly property QtObject large: QtObject {
            readonly property int width: theme.contentWidth * 0.50
            readonly property int height: theme.scaled(52)
            readonly property int fontSize: theme.fontSize.large
            readonly property int iconSize: theme.scaled(24)
        }

        readonly property QtObject row: QtObject {
            readonly property int width: theme.contentWidth
            readonly property int height: theme.scaled(52)
            readonly property int fontSize: theme.fontSize.medium
            readonly property int iconSize: theme.scaled(20)
        }

        readonly property QtObject rowCompact: QtObject {
            readonly property int width: theme.contentWidth
            readonly property int height: theme.scaled(44)
            readonly property int fontSize: theme.fontSize.normal
            readonly property int iconSize: theme.scaled(18)
        }

        readonly property QtObject primary: QtObject {
            readonly property color background: theme.colors.primary
            readonly property color hovered: theme.colors.primaryVariant
            readonly property color pressed: theme.colors.primaryDark
            readonly property color border: theme.colors.primary
            readonly property color text: theme.colors.textInverse
        }

        readonly property QtObject primarySoft: QtObject {
            readonly property color background: Qt.rgba(theme.colors.primary.r, theme.colors.primary.g, theme.colors.primary.b, theme.isNightMode ? 0.30 : 0.22)
            readonly property color hovered: Qt.rgba(theme.colors.primary.r, theme.colors.primary.g, theme.colors.primary.b, theme.isNightMode ? 0.40 : 0.30)
            readonly property color pressed: Qt.rgba(theme.colors.primary.r, theme.colors.primary.g, theme.colors.primary.b, theme.isNightMode ? 0.48 : 0.38)
            readonly property color border: Qt.rgba(theme.colors.primary.r, theme.colors.primary.g, theme.colors.primary.b, 0.60)
            readonly property color text: Qt.rgba(theme.colors.textPrimary.r, theme.colors.textPrimary.g, theme.colors.textPrimary.b, 0.85)
        }

        readonly property QtObject secondary: QtObject {
            readonly property color background: theme.colors.secondary
            readonly property color hovered: theme.isNightMode ? "#A78BFA" : "#8B5CF6"
            readonly property color pressed: theme.isNightMode ? "#7C3AED" : "#6D28D9"
            readonly property color border: theme.colors.secondary
            readonly property color text: theme.colors.textInverse
        }

        readonly property QtObject secondarySoft: QtObject {
            readonly property color background: Qt.rgba(theme.colors.secondary.r, theme.colors.secondary.g, theme.colors.secondary.b, theme.isNightMode ? 0.30 : 0.22)
            readonly property color hovered: Qt.rgba(theme.colors.secondary.r, theme.colors.secondary.g, theme.colors.secondary.b, theme.isNightMode ? 0.40 : 0.30)
            readonly property color pressed: Qt.rgba(theme.colors.secondary.r, theme.colors.secondary.g, theme.colors.secondary.b, theme.isNightMode ? 0.48 : 0.38)
            readonly property color border: Qt.rgba(theme.colors.secondary.r, theme.colors.secondary.g, theme.colors.secondary.b, 0.60)
            readonly property color text: Qt.rgba(theme.colors.textPrimary.r, theme.colors.textPrimary.g, theme.colors.textPrimary.b, 0.85)
        }

        readonly property QtObject success: QtObject {
            readonly property color background: theme.colors.success
            readonly property color hovered: "#34D399"
            readonly property color pressed: theme.isNightMode ? "#10B981" : "#059669"
            readonly property color border: theme.colors.success
            readonly property color text: theme.colors.textInverse
        }

        readonly property QtObject danger: QtObject {
            readonly property color background: theme.colors.error
            readonly property color hovered: theme.isNightMode ? "#FCA5A5" : "#F87171"
            readonly property color pressed: theme.isNightMode ? "#EF4444" : "#DC2626"
            readonly property color border: theme.colors.error
            readonly property color text: theme.colors.textInverse
        }

        readonly property QtObject dangerSoft: QtObject {
            readonly property color background: Qt.rgba(theme.colors.error.r, theme.colors.error.g, theme.colors.error.b, theme.isNightMode ? 0.24 : 0.16)
            readonly property color hovered: Qt.rgba(theme.colors.error.r, theme.colors.error.g, theme.colors.error.b, theme.isNightMode ? 0.34 : 0.24)
            readonly property color pressed: Qt.rgba(theme.colors.error.r, theme.colors.error.g, theme.colors.error.b, theme.isNightMode ? 0.42 : 0.32)
            readonly property color border: Qt.rgba(theme.colors.error.r, theme.colors.error.g, theme.colors.error.b, 0.60)
            readonly property color text: theme.colors.error
        }

        readonly property QtObject warning: QtObject {
            readonly property color background: theme.colors.warning
            readonly property color hovered: theme.isNightMode ? "#FFB84D" : "#FBBF24"
            readonly property color pressed: theme.isNightMode ? "#E69500" : "#D97706"
            readonly property color border: theme.colors.warning
            readonly property color text: theme.colors.textInverse
        }

        readonly property QtObject ghost: QtObject {
            readonly property color background: "transparent"
            readonly property color hovered: Qt.rgba(theme.colors.textPrimary.r, theme.colors.textPrimary.g, theme.colors.textPrimary.b, theme.isNightMode ? 0.12 : 0.08)
            readonly property color pressed: Qt.rgba(theme.colors.textPrimary.r, theme.colors.textPrimary.g, theme.colors.textPrimary.b, theme.isNightMode ? 0.20 : 0.14)
            readonly property color border: theme.colors.cardBorder
            readonly property color text: theme.colors.textPrimary
        }

        readonly property QtObject ghostPrimary: QtObject {
            readonly property color background: "transparent"
            readonly property color hovered: Qt.rgba(theme.colors.primary.r, theme.colors.primary.g, theme.colors.primary.b, theme.isNightMode ? 0.16 : 0.10)
            readonly property color pressed: Qt.rgba(theme.colors.primary.r, theme.colors.primary.g, theme.colors.primary.b, theme.isNightMode ? 0.26 : 0.18)
            readonly property color border: theme.colors.primary
            readonly property color text: theme.colors.primary
        }

        readonly property QtObject ghostDanger: QtObject {
            readonly property color background: "transparent"
            readonly property color hovered: Qt.rgba(theme.colors.error.r, theme.colors.error.g, theme.colors.error.b, theme.isNightMode ? 0.16 : 0.10)
            readonly property color pressed: Qt.rgba(theme.colors.error.r, theme.colors.error.g, theme.colors.error.b, theme.isNightMode ? 0.26 : 0.18)
            readonly property color border: Qt.rgba(theme.colors.error.r, theme.colors.error.g, theme.colors.error.b, 0.60)
            readonly property color text: theme.colors.error
        }
    }

    property QtObject card: QtObject {
        readonly property int sizeSmall: theme.scaled(100)
        readonly property int sizeMedium: theme.scaled(130)
        readonly property int sizeLarge: theme.scaled(180)
    }

    property QtObject navigation: QtObject {
        readonly property int barHeight: theme.scaled(56)
    }

    property QtObject panel: QtObject {
        readonly property int handleWidth: theme.scaled(40)
        readonly property int handleHeight: theme.scaled(4)
        readonly property int titleSize: theme.fontSize.large
        readonly property real maxHeightRatio: 0.75
    }

    property QtObject icon: QtObject {
        readonly property int small: theme.scaled(16)
        readonly property int medium: theme.scaled(24)
        readonly property int large: theme.scaled(32)
    }

    property QtObject icons: QtObject {
        readonly property string search: "qrc:/Themed/Icons/search.svg"
        readonly property string close: "qrc:/Themed/Icons/close.svg"
        readonly property string menu: "qrc:/Themed/Icons/menu.svg"
        readonly property string more: "qrc:/Themed/Icons/more-vertical.svg"
        readonly property string user: "qrc:/Themed/Icons/user.svg"
        readonly property string settings: "qrc:/Themed/Icons/settings.svg"
        readonly property string sliders: "qrc:/Themed/Icons/sliders.svg"
        readonly property string edit: "qrc:/Themed/Icons/edit.svg"
        readonly property string home: "qrc:/Themed/Icons/home.svg"
        readonly property string list: "qrc:/Themed/Icons/list.svg"
        readonly property string deck: "qrc:/Themed/Icons/deck.svg"
        readonly property string send: "qrc:/Themed/Icons/send.svg"
        readonly property string chat: "qrc:/Themed/Icons/chat.svg"
        readonly property string back: "qrc:/Themed/Icons/previous.svg"
        readonly property string next: "qrc:/Themed/Icons/next.svg"
        readonly property string chevronUp: "qrc:/Themed/Icons/chevron-up.svg"
        readonly property string chevronDown: "qrc:/Themed/Icons/chevron-down.svg"
        readonly property string chevronRight: "qrc:/Themed/Icons/chevron-right.svg"
        readonly property string expand: "qrc:/Themed/Icons/expand.svg"
        readonly property string collapse: "qrc:/Themed/Icons/collapse.svg"
        readonly property string info: "qrc:/Themed/Icons/info.svg"
        readonly property string success: "qrc:/Themed/Icons/success.svg"
        readonly property string warning: "qrc:/Themed/Icons/warning.svg"
        readonly property string error: "qrc:/Themed/Icons/error.svg"
        readonly property string check: "qrc:/Themed/Icons/check.svg"
        readonly property string plus: "qrc:/Themed/Icons/plus.svg"
        readonly property string minus: "qrc:/Themed/Icons/minus.svg"
        readonly property string remove: "qrc:/Themed/Icons/remove.svg"
        readonly property string trash: "qrc:/Themed/Icons/remove.svg"
        readonly property string star: "qrc:/Themed/Icons/star.svg"
        readonly property string starFull: "qrc:/Themed/Icons/star-full.svg"
        readonly property string starHalf: "qrc:/Themed/Icons/star-half.svg"
        readonly property string copy: "qrc:/Themed/Icons/copy.svg"
        readonly property string paste: "qrc:/Themed/Icons/import.svg"
        readonly property string speaker: "qrc:/Themed/Icons/speaker.svg"
        readonly property string translate: "qrc:/Themed/Icons/translate.svg"
        readonly property string globe: "qrc:/Themed/Icons/globe.svg"
        readonly property string curvedArrow: "qrc:/Themed/Icons/curved-arrow.svg"
        readonly property string calendar: "qrc:/Themed/Icons/calendar.svg"
        readonly property string timer: "qrc:/Themed/Icons/timer.svg"
        readonly property string aiApp: "qrc:/Themed/Icons/ai-app.svg"
    }
}
